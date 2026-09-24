#pragma once

#include <kernel/global.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ENTRY_MASK 0x1ff
#define ADDRESS_MASK ~0xfffUL

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#define PAGE_HUGE_SIZE 0x200000UL // 2MB

#define PML4_IDX(vaddr) ((vaddr >> 39) & ENTRY_MASK)
#define PDPT_IDX(vaddr) ((vaddr >> 30) & ENTRY_MASK)
#define PD_IDX(vaddr) ((vaddr >> 21) & ENTRY_MASK)
#define PT_IDX(vaddr) ((vaddr >> 12) & ENTRY_MASK)

// Standard 64-bit Paging definitions
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_USER (1ULL << 2) // Bit 2: 0 = Supervisor/Kernel, 1 = User-space
#define PAGE_HUGE (1ULL << 7) // Enable 2MB page
// Cache Control Flags
#define PAGE_PWT (1ULL << 3) // Bit 3: Write-Through caching
#define PAGE_PCD (1ULL << 4) // Bit 4: Cache Disable (Uncacheable)
// Bit 63: No-Execute. (Requires IA32_EFER.NXE to be enabled in your CPU
// initialization)
#define PAGE_NX (1ULL << 63)
// Unified Flag Combinations
#define PAGE_WRITE_BACK (PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE) // PCD=0, PWT=0
#define PAGE_UNCACHEABLE (PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT | PAGE_HUGE)

typedef enum { PAGE_4K, PAGE_2M } page_type_t;

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

bool mm_map_virt_addr(uint64_t *pml4, vaddr_t vaddr, paddr_t paddr, uint64_t flags, size_t sz, page_type_t type);

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
