#pragma once

/* 8x16 console font, one byte per pixel row, MSB = leftmost pixel.
 * Regenerate with: python3 tools/genfont.py <font.psf.gz> <out.c> <out.h>
 */

#include <stdint.h>

#define FONT8X16_W 8
#define FONT8X16_H 16
#define FONT8X16_GLYPHS 256
#define FONT8X16_BYTES 16

extern const uint8_t font8x16[FONT8X16_GLYPHS][FONT8X16_BYTES];
