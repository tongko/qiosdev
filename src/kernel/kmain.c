#include <kernel/gdt.h>
#include <kernel/global.h>
#include <kernel/idt.h>
#include <kernel/kmain.h>
#include <kernel/klog.h>
#include <kernel/mm.h>
#include <kernel/serial.h>
#include <kernel/tsc.h>
#include <libk/string.h>
#include <stdbool.h>

#define COLOR_ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

void kmain(bootinfo_t *bi) {
	// stop interrup
	__asm__ volatile("cli");

	// copy bootinfo so we can reclaim bootloader data later
	memcpy(&_bi, bi, sizeof(bootinfo_t));

	_tsc_start = bi->tsc_start;
	_tsc_hz = bi->tsc_hz;

	// Logging first: everything below this line can be reported, and printk()
	// only needs the TSC plus a polled COM1, no IDT or heap.
	log_init();
	serial_log_init();

	printk("qios: kernel entered, tsc = %llu Hz", (unsigned long long)_tsc_hz);
	printk("qios: framebuffer %ux%u stride=%u fmt=%u at %p",
			 (unsigned)bi->frame_buff.width,
			 (unsigned)bi->frame_buff.height,
			 (unsigned)bi->frame_buff.px_per_scanline,
			 (unsigned)bi->frame_buff.px_format,
			 (void *)bi->frame_buff.base_addr);

	// Get GDT working first
	gdt_init();
	idt_init();

	// memory and page frame allocator
	mm_init();
	printk("qios: memory subsystem ready, %llu MiB installed", (unsigned long long)(bi->total_installed_ram >> 20));

	uint32_t bg_color = COLOR_ARGB(0, 30, 40, 60);
	paint_background(bi, bg_color);
	printk("qios: painted %ux%u background", (unsigned)bi->frame_buff.width, (unsigned)bi->frame_buff.height);

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
