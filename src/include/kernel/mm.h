#pragma once

#include <kernel/global.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_ORDER 10

// ====================== Physical page (buddy backend) ======================
// allocate 2^order contiguous physical page frames
uintptr_t mm_alloc_pages(uint8_t order);
void mm_free_pages(uintptr_t pa, uint8_t order);

// allocate single 4k physical page (order 0)
static inline uintptr_t mm_alloc_page(void) {
	return mm_alloc_pages(0);
}
static inline void mm_free_page(uintptr_t pa) {
	mm_free_pages(pa, 0);
}

// ====================== Kernel heap (SLOB backend, kmalloc/kfree) ======================
void mm_heap_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(void *ptr);

// ====================== HHDM address translation ======================
void *hhdm_map_pa(uintptr_t pa);
uintptr_t hhdm_to_pa(void *va);

// ====================== MM subsystem init entry ======================
void mm_init(void);

// ====================== Debugging ====================================
// void mm_print_stats(void);
// size_t mm_get_total_phys_pages(void);
// size_t mm_get_free_phys_pages(void);
// size_t mm_get_heap_used(void);
