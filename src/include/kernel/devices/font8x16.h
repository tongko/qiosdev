#pragma once

/* Console font: one byte of coverage (0..255) per pixel, 8 bytes per row, cell 8x16.
 * Regenerate with tools/genfont.py. */

#include <stdint.h>

#define FONT8X16_W 8
#define FONT8X16_H 16
#define FONT8X16_GLYPHS 256
#define FONT8X16_STRIDE 1
#define FONT8X16_BYTES 128
#define FONT8X16_COVERAGE 1

extern const uint8_t font8x16[FONT8X16_GLYPHS][FONT8X16_BYTES];
