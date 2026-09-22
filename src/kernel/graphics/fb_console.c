// fb_console.c - 8x16 text console on top of the framebuffer device, plus the
// klog sink that renders the kernel log into it.
//
// Rendering never touches the hardware framebuffer: every glyph goes into the
// shadow buffer and only widens fb->dirty.  The single flush point is
// fb_log_flush(), which klog calls once per drain (i.e. once per printk in
// immediate mode).
#include <kernel/global.h>
#include <kernel/bmp.h>
#include <kernel/devices/fb.h>
#include <kernel/devices/font8x16.h>
#include <kernel/klog.h>
#include <kernel/mm.h>

// The console singleton.  Interrupt and panic paths reach the framebuffer
// through fb_panic_screen() instead of carrying a device pointer around.
static fb_device_t *g_fbcon;
static volatile bool g_fbcon_dead;

#if FONT8X16_COVERAGE
// src-over blend of fg/bg by an 8-bit coverage value (0 = bg, 255 = fg).
static uint32_t fb_blend_cov(uint32_t fg, uint32_t bg, uint32_t cov) {
	uint32_t inv = 255u - cov;
	uint32_t r = (((fg >> 16) & 0xFFu) * cov + ((bg >> 16) & 0xFFu) * inv + 127u) / 255u;
	uint32_t g = (((fg >> 8) & 0xFFu) * cov + ((bg >> 8) & 0xFFu) * inv + 127u) / 255u;
	uint32_t b = ((fg & 0xFFu) * cov + (bg & 0xFFu) * inv + 127u) / 255u;

	return (r << 16) | (g << 8) | b;
}
#endif

static void fb_draw_glyph(fb_device_t *fb, uint32_t cx, uint32_t cy, uint8_t ch, uint32_t fg, uint32_t bg) {
	uint32_t px = cx * FONT8X16_W;
	uint32_t py = cy * FONT8X16_H;
	const uint8_t *glyph = font8x16[ch];

	for (uint32_t row = 0; row < FONT8X16_H; row++) {
		uint32_t *dst = &fb->back[(size_t)(py + row) * fb->width + px];

#if FONT8X16_COVERAGE
		// One byte of coverage per pixel: blend the two colours.  That is what
		// gives a TTF bake the soft, full strokes it has in a terminal.
		const uint8_t *cov = &glyph[(size_t)row * FONT8X16_W];

		for (uint32_t col = 0; col < FONT8X16_W; col++) {
			dst[col] = (cov[col] == 0) ? bg : (cov[col] == 255 ? fg : fb_blend_cov(fg, bg, cov[col]));
		}
#else
		// FONT8X16_STRIDE bytes per row, MSB leftmost: a 16px wide bake (16x32
		// text) needs two bytes per row, an 8px one a single byte.
		const uint8_t *bits = &glyph[row * FONT8X16_STRIDE];

		for (uint32_t col = 0; col < FONT8X16_W; col++) {
			uint8_t mask = (uint8_t)(0x80u >> (col % 8));
			dst[col] = (bits[col / 8] & mask) != 0 ? fg : bg;
		}
#endif
	}

	fb_mark_dirty(fb, (int32_t)px, (int32_t)py, (int32_t)FONT8X16_W, (int32_t)FONT8X16_H);
}

static void fb_console_newline(fb_device_t *fb) {
	fb->cursor_x = 0;
	fb->cursor_y++;

	if (fb->cursor_y >= fb->rows) {
		fb_scroll_up(fb, FONT8X16_H); // scroll one character cell
		fb->cursor_y = fb->rows - 1;
	}
}

void fb_console_init(fb_device_t *fb, uint32_t fg, uint32_t bg) {
	g_fbcon = fb;
	g_fbcon_dead = false;
	fb->fg = fg;
	fb->bg = bg;
	fb->cols = fb->width / FONT8X16_W;
	fb->rows = fb->height / FONT8X16_H;
	fb_console_clear(fb);
}

void fb_console_clear(fb_device_t *fb) {
	fb_clear(fb, fb->bg);
	fb->cursor_x = 0;
	fb->cursor_y = 0;
}

void fb_console_putc(fb_device_t *fb, char c) {
	switch (c) {
	case '\n':
		fb_console_newline(fb);
		return;
	case '\r':
		fb->cursor_x = 0;
		return;
	case '\b':
		if (fb->cursor_x > 0) {
			fb->cursor_x--;
			fb_draw_glyph(fb, fb->cursor_x, fb->cursor_y, (uint8_t)' ', fb->fg, fb->bg);
		}
		return;
	case '\t': {
		uint32_t stop = (fb->cursor_x + 4u) & ~3u;
		if (stop > fb->cols) {
			stop = fb->cols;
		}
		while (fb->cursor_x < stop) {
			fb_draw_glyph(fb, fb->cursor_x, fb->cursor_y, (uint8_t)' ', fb->fg, fb->bg);
			fb->cursor_x++;
		}
		break;
	}
	default:
		if ((uint8_t)c < 0x20) {
			return; // ignore the remaining control characters
		}
		fb_draw_glyph(fb, fb->cursor_x, fb->cursor_y, (uint8_t)c, fb->fg, fb->bg);
		fb->cursor_x++;
		break;
	}

	if (fb->cursor_x >= fb->cols) {
		fb_console_newline(fb);
	}
}

void fb_console_puts(fb_device_t *fb, const char *s) {
	for (; *s != '\0'; s++) {
		fb_console_putc(fb, *s);
	}
}

void fb_console_write(fb_device_t *fb, const char *data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		fb_console_putc(fb, data[i]);
	}
}

// ---- klog sink ----

static log_subscriber_t fb_log_sub;

size_t fb_log_write(log_subscriber_t *sub, const char *data, size_t len) {
	if (g_fbcon_dead) {
		return len; // drop it rather than fault again
	}
	fb_console_write((fb_device_t *)sub->ctx, data, len);
	return len; // drawing into the shadow buffer never backpressures
}

void fb_log_flush(log_subscriber_t *sub) {
	if (g_fbcon_dead) {
		return;
	}
	fb_present((fb_device_t *)sub->ctx); // one blit for the whole batch
}

void fb_log_init(fb_device_t *fb, bool replay_from_start) {
	fb_log_sub.flush = fb_log_flush;
	log_subscriber_register(&fb_log_sub, "fbcon", fb_log_write, fb);

	if (replay_from_start) {
		// Point the sink back at the oldest byte still in the ring so the boot
		// output logged before the screen existed shows up too.
		fb_log_sub.pos = ringbuf_oldest(log_ring());
		log_subscribers_poll();
	}
}

// ---- panic path ----
// Reached from the exception handler with no device pointer in sight: the
// singleton above is the whole point.  Everything here is lock-free, allocation
// free and bounded.

void fb_console_mark_dead(void) {
	g_fbcon_dead = true;
}

static inline bmp_info_header_t *fb_get_header(uint8_t *bmp) {
	return (bmp_info_header_t *)(bmp + sizeof(bmp_header_t));
}

static void fb_draw_panic_crow(bootinfo_t *bi, fb_device_t *fb) {
	// draw panic-crow
	uint8_t *crow = (uint8_t *)bi->logo_bmp[0];
	bmp_info_header_t *bmp_h = fb_get_header(crow);
	int32_t width = bmp_h->bi_width;
	int32_t height = bmp_h->bi_height;
	int32_t y = (fb->height / 2) - (height / 3 * 2);
	int32_t x = (fb->width / 2) - (width / 2);
	draw_bmp(fb, crow, x, y);

	// draw panic-title
	uint8_t *title = (uint8_t *)bi->logo_bmp[1];
	bmp_h = fb_get_header(title);
	width = bmp_h->bi_width;
	x = (fb->width / 2) - (width / 2);
	y = (fb->height / 2) + (height / 2) + (bmp_h->bi_height / 2);
	height = bmp_h->bi_height;
	draw_bmp(fb, title, x, y);

	// draw panic-text
	uint8_t *text = (uint8_t *)bi->logo_bmp[2];
	bmp_h = fb_get_header(text);
	width = bmp_h->bi_width;
	x = (fb->width / 2) - (width / 2);
	y = y + height + (bmp_h->bi_height / 2);
	draw_bmp(fb, text, x, y);
}

void fb_panic_screen(const char *title, const char *detail) {
	fb_device_t *fb = g_fbcon;
	uint32_t banner = 0x00660000u; // red

	if (fb == NULL || g_fbcon_dead) {
		return; // the console never came up, or it is what faulted: serial only
	}

	// Clear screen
	fb_clear(fb, 0);

	// Two text rows of red with the title in white, then the message below.
	fb_fill_rect(fb, 0, 0, (int32_t)fb->width, (int32_t)(FONT8X16_H * 2), banner);

	fb->fg = 0x00FFFFFFu;
	fb->bg = banner;
	fb->cursor_x = 0;
	fb->cursor_y = 0;
	fb_console_puts(fb, title);

	fb->bg = 0x00000000u;
	fb->cursor_x = 0;
	fb->cursor_y = 2;
	fb_console_puts(fb, detail);

	// draw the panic crow
	fb_draw_panic_crow(&_bi, fb);

	fb_present_all(fb);
}
