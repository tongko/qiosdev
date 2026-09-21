#pragma once

#include <kernel/devices/fb.h>
#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)

typedef struct {
	uint16_t bf_type; //	'BM' = 0x4D42
	uint32_t bf_sz;
	uint16_t bf_resv1;
	uint16_t bf_resv2;
	uint32_t bf_off_bits; // offset to pixel data
} bmp_header_t;

typedef struct {
	uint32_t bi_sz; // must be 40 (0x28)
	int32_t bi_width;
	int32_t bi_height; // + = bottom-up, - = top-down
	uint16_t bi_planes;
	uint16_t bi_bit_count;
	uint32_t bi_compression;
	uint32_t bi_sz_image;
	int32_t bi_xpel_per_meter;
	int32_t bi_ypel_per_meter;
	uint32_t bi_clr_used;
	uint32_t bi_clr_important;
} bmp_info_header_t;

#pragma pack(pop)

void draw_bmp(fb_device_t *fbd, uint8_t *buf, int32_t offset_x, int32_t offset_y);