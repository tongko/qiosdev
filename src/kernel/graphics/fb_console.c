// fb_console.c - 8x16 text console on top of the framebuffer device, plus the
// klog sink that renders the kernel log into it.
//
// Rendering never touches the hardware framebuffer: every glyph goes into the
// shadow buffer and only widens fb->dirty.  The single flush point is
// fb_log_flush(), which klog calls once per drain (i.e. once per printk in
// immediate mode).
#include <kernel/devices/fb.h>
#include <kernel/devices/font8x16.h>
#include <kernel/klog.h>

static void fb_draw_glyph(fb_device_t *fb, uint32_t cx, uint32_t cy, uint8_t ch, uint32_t fg, uint32_t bg) {
	uint32_t px = cx * FONT8X16_W;
	uint32_t py = cy * FONT8X16_H;
	const uint8_t *glyph = font8x16[ch];

	for (uint32_t row = 0; row < FONT8X16_H; row++) {
		uint8_t bits = glyph[row]; // MSB is the leftmost pixel
		uint32_t *dst = &fb->back[(size_t)(py + row) * fb->width + px];

		for (uint32_t col = 0; col < FONT8X16_W; col++) {
			dst[col] = (bits & (0x80u >> col)) != 0 ? fg : bg;
		}
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
	fb_console_write((fb_device_t *)sub->ctx, data, len);
	return len; // drawing into the shadow buffer never backpressures
}

void fb_log_flush(log_subscriber_t *sub) {
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
