#include <bootinfo.h>
#include <stdbool.h>

#define COLOR_ARGB(a, r, g, b)                                                 \
	(((UINT32)(a) << 24) | ((UINT32)(r) << 16) | ((UINT32)(g) << 8) | (UINT32)(b))

void paint_background(bootinfo_t *bi, UINT32 color) {
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

void kmain(bootinfo_t *bi) {
	uint32_t bg_color = COLOR_ARGB(0, 30, 40, 60);
	paint_background(bi, bg_color);

	while (true) {
		__asm__ volatile("hlt");
	}
}