/*
 * Host-side test for the framebuffer driver (fb.c + fb_console.c).
 *
 * The kernel services the driver needs are stubbed so the shadow buffer and
 * the "hardware" framebuffer are ordinary malloc'd memory: mm_alloc_pages
 * returns a real allocation and hhdm_map_pa is the identity.  klog.c is the
 * production one, built against tests/mock/kernel/spinlock.h.
 */
#include <kernel/devices/fb.h>
#include <kernel/devices/font8x16.h>
#include <kernel/klog.h>
#include <kernel/tsc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 32
#define H 32
#define STRIDE 40 // deliberately larger than W: exercises row padding

static uint32_t hw[STRIDE * H];
static int failures;

#define CHECK(cond)                                                                            \
	do {                                                                                         \
		if (!(cond)) {                                                                            \
			failures++;                                                                             \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
		}                                                                                         \
	} while (0)

// ---- kernel stubs ----
bootinfo_t _bi;
uint64_t _tsc_start = 0;
uint64_t _tsc_hz = 1000000000ULL; // 1 tick == 1 ns

void *kzalloc(size_t n) {
	return calloc(1, n);
}

void kfree(void *p) {
	free(p);
}

uintptr_t mm_alloc_pages(uint8_t order) {
	(void)order;
	return (uintptr_t)calloc(1, 1u << 20);
}

void *hhdm_map_pa(uintptr_t pa) {
	return (void *)pa;
}

// ---- helpers ----
static uint32_t hw_px(int x, int y) {
	return hw[(size_t)y * STRIDE + (size_t)x];
}

static uint32_t back_px(fb_device_t *fb, int x, int y) {
	return fb->back[(size_t)y * fb->width + (size_t)x];
}

static int rect_empty(fb_device_t *fb) {
	return fb->dirty.x0 >= fb->dirty.x1 || fb->dirty.y0 >= fb->dirty.y1;
}

static void fresh(fb_device_t *fb) {
	memset(hw, 0xAA, sizeof hw);
	fb_console_clear(fb);
	fb_present_all(fb);
}

// ---- tests ----
static void test_init(fb_device_t *fb) {
	CHECK(fb != NULL);
	CHECK(fb->width == W && fb->height == H && fb->stride_px == STRIDE);
	CHECK(fb->back != NULL);
	CHECK(fb->cols == W / FONT8X16_W && fb->rows == H / FONT8X16_H);

	memset(hw, 0xAA, sizeof hw); // fb_init must not have touched it yet
	CHECK(hw_px(0, 0) == 0xAAAAAAAAu);
	fb_present_all(fb);
	CHECK(hw_px(0, 0) == 0);
	CHECK(hw_px(W - 1, H - 1) == 0);
	// present must copy exactly width pixels per row, not the padding
	CHECK(hw[0 * STRIDE + W] == 0xAAAAAAAAu);
	CHECK(hw[(H - 1) * STRIDE + W] == 0xAAAAAAAAu);
}

static void test_dirty_rect(fb_device_t *fb) {
	fresh(fb);
	CHECK(rect_empty(fb));

	fb_put_px(fb, 5, 5, 0x00FF0000u);
	CHECK(fb->dirty.x0 == 5 && fb->dirty.y0 == 5 && fb->dirty.x1 == 6 && fb->dirty.y1 == 6);

	fb_present(fb);
	CHECK(hw_px(5, 5) == 0x00FF0000u);
	CHECK(hw_px(4, 5) == 0 && hw_px(6, 5) == 0 && hw_px(5, 6) == 0);
	CHECK(hw[5 * STRIDE + W] == 0xAAAAAAAAu); // padding untouched
	CHECK(rect_empty(fb));                    // present cleared the dirty rect

	// out of bounds pixels are dropped, not written
	fb_put_px(fb, -1, -1, 0xFFFFFFFFu);
	fb_put_px(fb, W, H, 0xFFFFFFFFu);
	CHECK(rect_empty(fb));

	// the rect is the union of everything drawn since the last present
	fb_put_px(fb, 1, 1, 1u);
	fb_put_px(fb, 10, 20, 2u);
	CHECK(fb->dirty.x0 == 1 && fb->dirty.y0 == 1 && fb->dirty.x1 == 11 && fb->dirty.y1 == 21);
	fb_present(fb);
	CHECK(hw_px(1, 1) == 1u && hw_px(10, 20) == 2u);
	CHECK(hw_px(2, 2) == 0); // midpoint was never drawn
}

static void test_glyph(fb_device_t *fb) {
	fresh(fb);
	fb_console_putc(fb, 'A');

	// font8x16['A'] row 2 is 0x10, so column 3 is set and column 4 is not.
	CHECK(font8x16['A'][2] == 0x10);
	CHECK(back_px(fb, 3, 2) == fb->fg);
	CHECK(back_px(fb, 4, 2) == fb->bg);
	CHECK(back_px(fb, 0, 0) == fb->bg); // 'A' row 0 is empty

	// the cursor advanced one cell
	CHECK(fb->cursor_x == 1 && fb->cursor_y == 0);
}

static int band_matches(fb_device_t *fb, int y0, const uint8_t *glyph) {
	for (int r = 0; r < FONT8X16_H; r++) {
		for (int c = 0; c < FONT8X16_W; c++) {
			uint32_t want = (glyph[r] & (0x80u >> c)) != 0 ? fb->fg : fb->bg;
			if (back_px(fb, c, y0 + r) != want) {
				return 0;
			}
		}
	}
	return 1;
}

static void test_scroll(fb_device_t *fb) {
	fresh(fb);

	// rows == 2, so the third line forces a scroll of one text row.
	fb_console_puts(fb, "A\nB\nC\n");

	CHECK(band_matches(fb, 0, font8x16['C'])); // 'C' scrolled up to the top row
	for (int y = FONT8X16_H; y < H; y++) {
		for (int x = 0; x < W; x++) {
			CHECK(back_px(fb, x, y) == fb->bg); // bottom row is blank
		}
	}
	CHECK(fb->cursor_y == fb->rows - 1); // cursor parked on the last row
	CHECK(band_matches(fb, FONT8X16_H, font8x16[' ']));
}

static void test_log_sink(fb_device_t *fb) {
	int lit = 0;

	fresh(fb);
	log_init();
	fb_log_init(fb, false); // no replay

	// One printk -> one drain -> one flush: the line must already be on the
	// "hardware" framebuffer with no explicit present() from us.
	printk("hello");

	for (int y = 0; y < FONT8X16_H; y++) {
		for (int x = 0; x < W; x++) {
			if (hw_px(x, y) != 0) {
				lit++;
			}
		}
	}
	CHECK(lit > 0);
	CHECK(rect_empty(fb)); // the flush hook cleared it
}

int main(void) {
	fb_device_t *fb = fb_init(hw, W, H, STRIDE, 32);
	test_init(fb);
	if (fb == NULL) {
		printf("test_fb: fb_init failed, aborting\n");
		return 1;
	}

	test_dirty_rect(fb);
	test_glyph(fb);
	test_scroll(fb);
	test_log_sink(fb);

	if (failures == 0) {
		printf("test_fb: all tests passed\n");
	} else {
		printf("test_fb: %d failure(s)\n", failures);
	}
	return failures != 0;
}
