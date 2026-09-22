// bmp.c - 32-bit BMP blitter for the panic screen.
//
// The images the bootloader hands over are Photoshop 32-bit BMPs with an alpha
// mask (BI_BITFIELDS, 56-byte V3 header, masks 0x00FF0000 / 0x0000FF00 /
// 0x000000FF / 0xFF000000).  They are anti-aliased, so the alpha byte *is* part
// of the picture:
//
//   - alpha 0 is the transparent background; painting it opaque gives a black
//     box around the artwork,
//   - the soft edges of the outlines and the white sparkles carry low alpha;
//     painting them opaque turns delicate strokes into a hard, blotchy halo.
//
// So pixels are composited over whatever is already in the shadow buffer, which
// is what an image viewer (or kitten icat) does.  Everything is drawn straight
// into fb->back, clipped once up front and marked dirty once for the whole
// image, rather than through fb_put_px() per pixel.
#include <kernel/bmp.h>
#include <kernel/global.h>
#include <kernel/klog.h>
#include <libk/string.h>

// Number of trailing zero bits in a channel mask.
static uint32_t mask_shift(uint32_t mask) {
	uint32_t shift = 0;

	while (mask != 0 && (mask & 1u) == 0) {
		mask >>= 1;
		shift++;
	}
	return shift;
}

// Number of set bits in a channel mask.
static uint32_t mask_bits(uint32_t mask) {
	uint32_t bits = 0;

	while (mask != 0) {
		bits += mask & 1u;
		mask >>= 1;
	}
	return bits;
}

// Extract one channel and scale it to 0..255 (handles 5-6-5 style layouts too).
static uint32_t channel(uint32_t px, uint32_t mask, uint32_t shift, uint32_t bits) {
	uint32_t v;

	if (mask == 0) {
		return 0;
	}

	v = (px & mask) >> shift;
	if (bits != 0 && bits < 8) {
		v = (v * 255u) / ((1u << bits) - 1u);
	}
	return v & 0xFFu;
}

// src over dst, both 0x00RRGGBB, a in 0..255.
static uint32_t blend(uint32_t src, uint32_t dst, uint32_t a) {
	uint32_t inv = 255u - a;
	uint32_t r = (((src >> 16) & 0xFFu) * a + ((dst >> 16) & 0xFFu) * inv + 127u) / 255u;
	uint32_t g = (((src >> 8) & 0xFFu) * a + ((dst >> 8) & 0xFFu) * inv + 127u) / 255u;
	uint32_t b = ((src & 0xFFu) * a + (dst & 0xFFu) * inv + 127u) / 255u;

	return (r << 16) | (g << 8) | b;
}

void draw_bmp(fb_device_t *fbd, uint8_t *buf, int32_t offset_x, int32_t offset_y) {
	bmp_header_t *bh;
	bmp_info_header_t *ih;
	uint32_t rmask = 0x00FF0000u;
	uint32_t gmask = 0x0000FF00u;
	uint32_t bmask = 0x000000FFu;
	uint32_t amask = 0; // no alpha channel: every pixel is opaque
	uint32_t rsh, gsh, bsh, ash;
	uint32_t rbits, gbits, bbits, abits;
	int32_t w, h, x0, x1, y0, y1;
	size_t stride;
	const uint8_t *px;
	bool bottom_up;

	if (fbd == NULL || buf == NULL) {
		return;
	}

	bh = (bmp_header_t *)buf;
	if (bh->bf_type != 0x4D42) {
		printk("bmp: not a BMP (bf_type=0x%x)", (unsigned)bh->bf_type);
		return;
	}

	ih = (bmp_info_header_t *)(buf + sizeof(bmp_header_t));
	if (ih->bi_sz < 40 || ih->bi_bit_count != 32 || (ih->bi_compression != 0 && ih->bi_compression != 3)) {
		printk("bmp: unsupported (bi_sz=%u bits=%u compression=%u)", (unsigned)ih->bi_sz,
					 (unsigned)ih->bi_bit_count, (unsigned)ih->bi_compression);
		return;
	}

	// With BI_BITFIELDS the channel layout comes from the masks that follow the
	// 40-byte core header.  With BI_RGB the 4th byte is reserved, not opacity,
	// so those pixels stay opaque.
	if (ih->bi_compression == 3) {
		const uint8_t *m = buf + 14 + 40;

		memcpy(&rmask, m + 0, 4);
		memcpy(&gmask, m + 4, 4);
		memcpy(&bmask, m + 8, 4);
		if (bh->bf_off_bits >= 14 + 40 + 16) {
			memcpy(&amask, m + 12, 4);
		}
	}

	if (rmask == 0 || gmask == 0 || bmask == 0) {
		printk("bmp: no usable channel masks");
		return;
	}

	rsh = mask_shift(rmask);
	gsh = mask_shift(gmask);
	bsh = mask_shift(bmask);
	ash = mask_shift(amask);
	rbits = mask_bits(rmask);
	gbits = mask_bits(gmask);
	bbits = mask_bits(bmask);
	abits = mask_bits(amask);

	// A positive height means bottom-up rows, a negative one means top-down.
	w = ih->bi_width;
	h = ih->bi_height;
	bottom_up = h > 0;
	if (!bottom_up) {
		h = -h;
	}
	if (w <= 0 || h <= 0) {
		return;
	}

	px = buf + bh->bf_off_bits;
	stride = (size_t)w * 4; // 32 bpp rows are always 4-byte aligned

	// Clip once, then walk only what is actually on screen.
	x0 = offset_x < 0 ? -offset_x : 0;
	y0 = offset_y < 0 ? -offset_y : 0;
	x1 = w;
	y1 = h;
	if (offset_x + x1 > (int32_t)fbd->width) {
		x1 = (int32_t)fbd->width - offset_x;
	}
	if (offset_y + y1 > (int32_t)fbd->height) {
		y1 = (int32_t)fbd->height - offset_y;
	}
	if (x0 >= x1 || y0 >= y1) {
		return;
	}

	for (int32_t row = y0; row < y1; row++) {
		int32_t src_row = bottom_up ? (h - 1 - row) : row;
		const uint8_t *src = px + (size_t)src_row * stride;
		uint32_t *dst = &fbd->back[(size_t)(offset_y + row) * fbd->width + (size_t)offset_x];

		for (int32_t x = x0; x < x1; x++) {
			uint32_t p;
			uint32_t a;
			uint32_t c;

			memcpy(&p, src + (size_t)x * 4, 4);

			a = (amask != 0) ? channel(p, amask, ash, abits) : 255u;
			if (a == 0) {
				continue; // fully transparent: leave what is already on screen
			}

			// 0x00RRGGBB is what the BGRx framebuffer wants.
			c = (channel(p, rmask, rsh, rbits) << 16) | (channel(p, gmask, gsh, gbits) << 8) |
					channel(p, bmask, bsh, bbits);

			dst[x] = (a == 255) ? c : blend(c, dst[x], a);
		}
	}

	// One dirty update for the whole image instead of one per pixel.
	fb_mark_dirty(fbd, offset_x, offset_y, w, h);
}
