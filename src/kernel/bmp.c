#include <kernel/bmp.h>
#include <kernel/global.h>
#include <kernel/klog.h>
#include <libk/string.h>

void draw_bmp(fb_device_t *fbd, uint8_t *buf, int32_t offset_x, int32_t offset_y) {
	bmp_header_t *bmph = (bmp_header_t *)buf;
	if (bmph->bf_type != 0x4D42) {
		printk("paint_logo: Invalid BMP file.");
		return;
	}

	bmp_info_header_t *ih = (bmp_info_header_t *)(buf + sizeof(bmp_header_t));
	if (ih->bi_sz < 40 || ih->bi_bit_count != 32 || !(ih->bi_compression == 0 || ih->bi_compression == 3)) {
		printk("paint_logo: Invalid BMP file construct: bi_sz=%d, bi_bit_count=%d, bi_compression=%d",
				 ih->bi_sz,
				 ih->bi_bit_count,
				 ih->bi_compression);
		return;
	}

	uint8_t *px_base = buf + bmph->bf_off_bits;
	uint16_t img_w = ih->bi_width, img_h = ih->bi_height;
	uint32_t stride = img_w * (ih->bi_bit_count / 8);
	bool bottom_up = img_h > 0;

	for (int32_t bmp_y = 0; bmp_y < img_h; bmp_y++) {
		// map BMP bottom-up row to screen Y coordinate
		int32_t fb_y = bottom_up ? (img_h - 1 - bmp_y) : bmp_y;
		int32_t py = offset_y + fb_y;

		// pointer to start of this BMP scanline
		uint8_t *scanline = px_base + bmp_y * stride;

		for (uint32_t x = 0; x < img_w; x++) {
			// pack into 0xAARRGGBB format
			// uint32_t color = (*(uint32_t *)(scanline + x * 4)) | 0xff000000U;
			uint32_t color;
			memcpy(&color, scanline + (x * 4), sizeof(color));

			// draw pixel at (x, fb_y)
			int32_t px = offset_x + x;
			if ((int32_t)x < 0 || fb_y < 0 || x >= fbd->width || (uint32_t)fb_y >= fbd->height) {
				continue;
			}
			fb_put_px(fbd, px, py, color);
		}
	}

	fb_present_all(fbd);
}