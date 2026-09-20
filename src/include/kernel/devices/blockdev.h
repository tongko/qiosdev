#pragma once

// Block device abstraction (storage layer)

#include <kernel/devices/device.h>
#include <stdint.h>
#include <stddef.h>

typedef struct block_device block_device_t;

typedef struct {
	// @param lba: logical block address (0 - indexed)
	// @param buf: destination buffer
	// @param count: number of blocks to read
	// @return 0 success, negative error code
	int (*read_blocks)(block_device_t *dev, uint64_t lba, void *buf, size_t n);
	int (*write_blocks)(block_device_t *dev, uint64_t lba, const void *buf, size_t n);
} block_device_ops_t;

struct block_device {
	device_t base;
	block_device_ops_t *ops;
	uint64_t block_sz;
	uint64_t total_blks;
	uint64_t part_offset;
};

// Helper
int blockdev_read(block_device_t *bdev, uint64_t lba, void *buf, size_t n);
int blockdev_write(block_device_t *bdev, uint64_t lba, const void *buf, size_t n);