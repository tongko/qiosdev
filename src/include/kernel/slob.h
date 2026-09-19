#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct slob_block {
	size_t size;				 // size of payload (exclude header)
	struct slob_block *next; // next free block in singly linked list
} slob_block_t;

// One SLOB page: each 4k page managed by SLOB has a page header
typedef struct slob_page {
	struct slob_page *next_page;
	slob_block_t *free_list; // free block list inside this page
	size_t used_bytes;		 // total used bytes in this page
} slob_page_t;

// Global: list of all slob pages we allocated from buddy
extern slob_page_t *_slob_page_list;

void slob_init(void);
void *slob_alloc(size_t size);
void slob_free(void *ptr);