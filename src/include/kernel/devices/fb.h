#pragma once

// ==== Framebuffer driver + text console (built-in, for debug output) ====
//
// Double buffered on purpose: the real framebuffer is memory-mapped device
// memory (UC/WC), where per-pixel writes are slow, while the shadow buffer in
// fb_device::back is ordinary write-back RAM.  Every drawing op writes to
// `back` and only widens fb_device::dirty; the hardware framebuffer is touched
// in exactly one place, fb_present(), which copies the dirty rectangle over
// one linear memcpy per row.
//
// So the answer to "when do I flush?" is: at the end of a batch, never inside
// a drawing op.  For the kernel log that batch boundary is a klog drain - see
// fb_log_flush(), which printk() reaches once per line.

#include <kernel/devices/device.h>
#include <kernel/klog.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct fb_device fb_device_t;

// Half-open pixel rectangle [x0, x1) x [y0, y1).  Empty when x0 >= x1.
typedef struct {
	int32_t x0;
	int32_t y0;
	int32_t x1;
	int32_t y1;
} fb_rect_t;

/*
 * Hardware backend.  Only these ops touch the real framebuffer; everything
 * else in this driver is a software rasterizer over the shadow buffer.
 */
typedef struct fb_ops {
	// Copy back -> the real framebuffer.  dirty == NULL means "whole screen".
	void (*present)(fb_device_t *fb, const fb_rect_t *dirty);
	// Optional hardware scroll (NULL = software scroll of the shadow buffer).
	void (*scroll)(fb_device_t *fb, uint32_t lines);
} fb_ops_t;

struct fb_device {
	device_t base;

	uint32_t *vaddr;   // hardware framebuffer (from bootinfo)
	uint32_t *back;    // shadow buffer, width-strided: draw here
	uint32_t width;    // pixels
	uint32_t height;   // pixels
	uint32_t stride_px; // pixels per hardware scanline (>= width)
	uint32_t bpp;       // bits per pixel, must be 32
	uint32_t format;    // EFI pixel format (1 = BlueGreenRedReserved)

	fb_rect_t dirty; // pixels changed since the last present

	// text console state (fb_console.c)
	uint32_t cols;      // width  / FONT8X16_W
	uint32_t rows;      // height / FONT8X16_H
	uint32_t cursor_x;  // in character cells
	uint32_t cursor_y;
	uint32_t fg;
	uint32_t bg;

	const fb_ops_t *ops;
};

// ==== device ====
// stride_px is in PIXELS (bootinfo's px_per_scanline), not bytes.
fb_device_t *fb_init(void *fb_vaddr, uint32_t w, uint32_t h, uint32_t stride_px, uint32_t bpp);

// ==== presentation ====
void fb_mark_dirty(fb_device_t *fb, int32_t x, int32_t y, int32_t w, int32_t h);
void fb_present(fb_device_t *fb);     // flush fb->dirty, then clear it
void fb_present_all(fb_device_t *fb); // force a whole-screen flush

// ==== software rasterizers (all draw into fb->back) ====
void fb_put_px(fb_device_t *fb, int32_t x, int32_t y, uint32_t color);
void fb_fill_rect(fb_device_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void fb_clear(fb_device_t *fb, uint32_t color);
void fb_scroll_up(fb_device_t *fb, uint32_t lines);

// ==== text console ====
void fb_console_init(fb_device_t *fb, uint32_t fg, uint32_t bg);
void fb_console_clear(fb_device_t *fb);
void fb_console_putc(fb_device_t *fb, char c);
void fb_console_puts(fb_device_t *fb, const char *s);
void fb_console_write(fb_device_t *fb, const char *data, size_t len);

// ==== klog sink: renders into the shadow buffer, flushes once per drain ====
size_t fb_log_write(log_subscriber_t *sub, const char *data, size_t len);
void fb_log_flush(log_subscriber_t *sub);
// Register "fbcon" as a log sink.  replay_from_start replays the boot log that
// is still in the ring, instead of only showing lines logged from now on.
void fb_log_init(fb_device_t *fb, bool replay_from_start);
