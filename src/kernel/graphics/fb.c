// fb.c - framebuffer device: shadow buffer, rasterizers, presentation.
#include <kernel/buddy.h>
#include <kernel/devices/fb.h>
#include <kernel/klog.h>
#include <kernel/mm.h>
#include <libk/string.h>

static void fb_rect_clear(fb_device_t *fb) {
	fb->dirty = (fb_rect_t){(int32_t)fb->width, (int32_t)fb->height, 0, 0};
}

static bool fb_rect_empty(const fb_rect_t *r) {
	return r->x0 >= r->x1 || r->y0 >= r->y1;
}

// ---- backend: linear GOP framebuffer ----

static void gop_present(fb_device_t *fb, const fb_rect_t *dirty) {
	int32_t x0 = 0;
	int32_t y0 = 0;
	int32_t x1 = (int32_t)fb->width;
	int32_t y1 = (int32_t)fb->height;

	if (dirty != NULL) {
		x0 = dirty->x0;
		y0 = dirty->y0;
		x1 = dirty->x1;
		y1 = dirty->y1;
	}

	if (x0 < 0) {
		x0 = 0;
	}
	if (y0 < 0) {
		y0 = 0;
	}
	if (x1 > (int32_t)fb->width) {
		x1 = (int32_t)fb->width;
	}
	if (y1 > (int32_t)fb->height) {
		y1 = (int32_t)fb->height;
	}

	for (int32_t y = y0; y < y1; y++) {
		memcpy(&fb->vaddr[(size_t)y * fb->stride_px + (size_t)x0],
				 &fb->back[(size_t)y * fb->width + (size_t)x0],
				 (size_t)(x1 - x0) * sizeof(uint32_t));
	}
}

static const fb_ops_t _gop_ops = {
	.present = gop_present,
	.scroll = NULL, // software scroll of the shadow buffer
};

// ---- device ----

fb_device_t *fb_init(void *fb_vaddr, uint32_t w, uint32_t h, uint32_t stride_px, uint32_t bpp) {
	if (fb_vaddr == NULL || w == 0 || h == 0 || bpp != 32) {
		printk("fb: invalid arguments (%p %llux%llu stride=%llu bpp=%llu)",
				 fb_vaddr,
				 (unsigned long long)w,
				 (unsigned long long)h,
				 (unsigned long long)stride_px,
				 (unsigned long long)bpp);
		return NULL;
	}
	if (stride_px < w) {
		printk("fb: stride %llu is smaller than width %llu", (unsigned long long)stride_px, (unsigned long long)w);
		return NULL;
	}

	fb_device_t *fb = kzalloc(sizeof(fb_device_t));
	if (fb == NULL) {
		printk("fb: out of memory for the device");
		return NULL;
	}

	// Shadow buffer: contiguous physical pages reached through the HHDM.  A
	// static array would bloat .bss by several MiB for nothing.
	size_t bytes = (size_t)w * h * sizeof(uint32_t);
	uint8_t order = 0;
	while (order < MAX_ORDER && ((size_t)PAGE_SIZE << order) < bytes) {
		order++;
	}

	uintptr_t pa = mm_alloc_pages(order);
	if (pa == 0) {
		printk("fb: could not allocate %llu bytes for the shadow buffer", (unsigned long long)bytes);
		kfree(fb);
		return NULL;
	}

	fb->base.name = "fb0";
	fb->vaddr = fb_vaddr;
	fb->back = hhdm_map_pa(pa);
	fb->width = w;
	fb->height = h;
	fb->stride_px = stride_px;
	fb->bpp = bpp;
	fb->ops = &_gop_ops;
	fb_rect_clear(fb);

	// Console geometry + a first wipe of the shadow buffer.
	fb_console_init(fb, COLOR_ARGB(0, 100, 100, 100), COLOR_ARGB(0, 0, 0, 0));

	printk("fb: %llux%llu, stride %llu px, shadow at 0x%llx (%llu KiB), %llux%llu text cells",
			 (unsigned long long)w,
			 (unsigned long long)h,
			 (unsigned long long)stride_px,
			 (unsigned long long)pa,
			 (unsigned long long)(bytes >> 10),
			 (unsigned long long)fb->cols,
			 (unsigned long long)fb->rows);
	return fb;
}

// ---- presentation ----

void fb_mark_dirty(fb_device_t *fb, int32_t x, int32_t y, int32_t w, int32_t h) {
	int32_t x0 = x;
	int32_t y0 = y;
	int32_t x1 = x + w;
	int32_t y1 = y + h;

	if (w <= 0 || h <= 0) {
		return;
	}

	// Clip to the screen first.
	if (x0 < 0) {
		x0 = 0;
	}
	if (y0 < 0) {
		y0 = 0;
	}
	if (x1 > (int32_t)fb->width) {
		x1 = (int32_t)fb->width;
	}
	if (y1 > (int32_t)fb->height) {
		y1 = (int32_t)fb->height;
	}
	if (x1 <= x0 || y1 <= y0) {
		return;
	}

	if (fb_rect_empty(&fb->dirty)) {
		fb->dirty = (fb_rect_t){x0, y0, x1, y1};
		return;
	}

	if (x0 < fb->dirty.x0) {
		fb->dirty.x0 = x0;
	}
	if (y0 < fb->dirty.y0) {
		fb->dirty.y0 = y0;
	}
	if (x1 > fb->dirty.x1) {
		fb->dirty.x1 = x1;
	}
	if (y1 > fb->dirty.y1) {
		fb->dirty.y1 = y1;
	}
}

void fb_present(fb_device_t *fb) {
	if (fb_rect_empty(&fb->dirty)) {
		return; // nothing drawn since the last flush
	}
	if (fb->ops != NULL && fb->ops->present != NULL) {
		fb->ops->present(fb, &fb->dirty);
	}
	fb_rect_clear(fb);
}

void fb_present_all(fb_device_t *fb) {
	if (fb->ops != NULL && fb->ops->present != NULL) {
		fb->ops->present(fb, NULL);
	}
	fb_rect_clear(fb);
}

// ---- rasterizers (shadow buffer only) ----

void fb_put_px(fb_device_t *fb, int32_t x, int32_t y, uint32_t color) {
	if (x < 0 || y < 0 || x >= (int32_t)fb->width || y >= (int32_t)fb->height) {
		return;
	}

	fb->back[(size_t)y * fb->width + (size_t)x] = color;
	fb_mark_dirty(fb, x, y, 1, 1);
}

void fb_fill_rect(fb_device_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	int32_t x0 = x;
	int32_t y0 = y;
	int32_t x1 = x + w;
	int32_t y1 = y + h;

	if (w <= 0 || h <= 0) {
		return;
	}
	if (x0 < 0) {
		x0 = 0;
	}
	if (y0 < 0) {
		y0 = 0;
	}
	if (x1 > (int32_t)fb->width) {
		x1 = (int32_t)fb->width;
	}
	if (y1 > (int32_t)fb->height) {
		y1 = (int32_t)fb->height;
	}
	if (x1 <= x0 || y1 <= y0) {
		return;
	}

	for (int32_t row = y0; row < y1; row++) {
		uint32_t *dst = &fb->back[(size_t)row * fb->width + (size_t)x0];
		for (int32_t i = 0; i < x1 - x0; i++) {
			dst[i] = color;
		}
	}

	fb_mark_dirty(fb, x0, y0, x1 - x0, y1 - y0);
}

void fb_clear(fb_device_t *fb, uint32_t color) {
	fb_fill_rect(fb, 0, 0, (int32_t)fb->width, (int32_t)fb->height, color);
}

void fb_scroll_up(fb_device_t *fb, uint32_t lines) {
	if (lines == 0) {
		return;
	}
	if (lines >= fb->height) {
		fb_clear(fb, fb->bg);
		return;
	}

	// Row by row: each copy is between distinct rows, so memcpy's
	// non-overlap requirement is respected.
	size_t row_bytes = (size_t)fb->width * sizeof(uint32_t);
	for (uint32_t y = lines; y < fb->height; y++) {
		memcpy(&fb->back[(size_t)(y - lines) * fb->width], &fb->back[(size_t)y * fb->width], row_bytes);
	}

	// Everything moved, so the whole screen has to be presented, not just the
	// freshly cleared band at the bottom.
	fb_mark_dirty(fb, 0, 0, (int32_t)fb->width, (int32_t)fb->height);

	fb_fill_rect(fb, 0, (int32_t)(fb->height - lines), (int32_t)fb->width, (int32_t)lines, fb->bg);
}
