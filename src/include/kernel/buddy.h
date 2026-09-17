#pragma once

#include <kernel/global.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096
#define MAX_ORDER 11
#define PAGE_SHIFT 12 // log2(4096)

// Represent one 4KB physical frame
typedef struct page page_t;
struct page {
	bool is_free;
	int order; // only valid if on free list
	page_t *next;
};

// HHDM macros, adjust your offset
#define HHDM_OFFSET 0xFFFF800000000000
#define HHDM(pa) ((void *)((uintptr_t)(pa) + HHDM_OFFSET))
#define HHDM_INV(va) ((uintptr_t)(va) - HHDM_OFFSET)

// Convert physical address to page index
static inline size_t pa_to_idx(uintptr_t pa) { return pa >> PAGE_SHIFT; }
// Convert page index to physical address
static inline uintptr_t idx_to_pa(size_t idx) {
	return (uintptr_t)idx << PAGE_SHIFT;
}

uintptr_t buddy_alloc(int req_order);
void buddy_free(uintptr_t pa, int order);
void buddy_add_range(uintptr_t start_pa, uintptr_t end_pa);
void init_buddy(mem_descriptor_t *map, size_t msz, size_t dsz);
