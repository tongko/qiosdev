#include <kernel/gdt.h>
#include <kernel/global.h>
#include <kernel/idt.h>
#include <kernel/kmain.h>
#include <kernel/klog.h>
#include <kernel/mm.h>
#include <kernel/serial.h>
#include <kernel/tsc.h>
#include <kernel/devices/device.h>
#include <kernel/devices/fb.h>
#include <kernel/bmp.h>
#include <libk/string.h>
#include <stdbool.h>

void kmain(bootinfo_t *bi) {
	// stop interrup
	__asm__ volatile("cli");

	// copy bootinfo so we can reclaim bootloader data later
	memcpy(&_bi, bi, sizeof(bootinfo_t));

	_tsc_start = bi->tsc_start ? bi->tsc_start : rdtsc();
	_tsc_hz = bi->tsc_hz;

	// Logging first: everything below this line can be reported, and printk()
	// only needs the TSC plus a polled COM1, no IDT or heap.
	log_init();
	serial_log_init();

	printk("qios: kernel entered, tsc = %llu Hz", (unsigned long long)_tsc_hz);
	printk("qios: bootinfo PML4=0x%lx", bi->pml4_paddr);
	framebuffer_t *fb = &bi->frame_buff;
	printk("qios: framebuffer %ux%u stride=%u fmt=%u at %p",
			 (unsigned)fb->width,
			 (unsigned)fb->height,
			 (unsigned)fb->px_per_scanline,
			 (unsigned)fb->px_format,
			 (void *)fb->base_addr);

	// Get GDT working first
	gdt_init();
	idt_init();

	// memory and page frame allocator
	mm_init();
	printk("qios: memory subsystem ready, %llu MiB installed", (unsigned long long)(bi->total_installed_ram >> 20));

	// init device root
	static device_t root;
	dev_init_root(&root);

	// built-in framebuffer driver (debug console)
	fb_device_t *fbd = fb_init(fb->base_addr, fb->width, fb->height, fb->px_per_scanline, 32);
	uint32_t bg_color = COLOR_ARGB(0xFF, 0, 0, 0);

	if (fbd != NULL) {
		dev_add_child(&root, &fbd->base);

		// Paint the background into the shadow buffer, then push it once.
		fb_clear(fbd, bg_color);
		fb_present_all(fbd);

		// Attach the screen to the log and replay whatever is still in the
		// ring, so the boot output logged above appears on screen as well.
		fb_log_init(fbd, true);
		printk("qios: framebuffer console ready");
	} else {
		paint_background(bi, bg_color);
		printk("qios: no framebuffer console, painted the background directly");
	}

	// volatile uint64_t *bad_ptr = (volatile uint64_t *)0xdeadbeef0000;
	// uint64_t val = *bad_ptr;
	// mm_free_page(val);

	while (true) {
		__asm__ volatile("hlt");
	}
}

void paint_background(bootinfo_t *bi, uint32_t color) {
	uint32_t *fb_base = bi->frame_buff.base_addr;

	uint32_t width = bi->frame_buff.width;
	uint32_t height = bi->frame_buff.height;
	uint32_t stride = bi->frame_buff.px_per_scanline;

	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			uint32_t px_idx = (y * stride) + x;
			fb_base[px_idx] = color;
		}
	}
}
