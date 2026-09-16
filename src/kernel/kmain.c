#include <kernel/global.h>
#include <kernel/kmain.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mm.h>
#include <stdbool.h>
#include <libk/string.h>

#define COLOR_ARGB(a, r, g, b)                                                 \
	(((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) |      \
	 (uint32_t)(b))

void kmain(bootinfo_t *bi) {
	// stop interrup
	__asm__ volatile("cli");

	// copy bootinfo so we can reclaim bootloader data later
	memcpy(_bootinfo, bi, sizeof(bootinfo_t));

	// Get GDT working first
	gdt_init();
	idt_init();

	// memory and page frame allocator
	mem_init((mmap_t *)&bi->mem_map);

	uint32_t bg_color = COLOR_ARGB(0, 30, 40, 60);
	paint_background(bi, bg_color);

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
