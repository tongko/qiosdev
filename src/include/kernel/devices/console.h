#pragma once

#include <kernel/devices/fb.h>
#include <stdint.h>

typedef struct {
	fb_device_t *fb;
	uint32_t cursor_x;
	uint32_t cursor_y;
	uint32_t fg_color;
	uint32_t bg_color;
	uint32_t cols;
	uint32_t rows;
} console_t;

console_t *console_init(fb_device_t *fb);
void console_putchar(console_t *con, char c);
void console_write(console_t *con, const char *s, size_t n);
void console_clear(console_t *con);
