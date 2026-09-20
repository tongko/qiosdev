#include "kernel/mm.h"
#include "kernel/buddy.h"
#include "kernel/global.h"
#include "kernel/slob.h"
#include "libk/string.h"

// Called once at kernel early boot
void mm_init(void) {
	buddy_init(_bi.mem_map.map, _bi.mem_map.map_size,
				  _bi.mem_map.desc_size); // init physical page frame allocator
	mm_heap_init();						  // init slob heap
}

// ---------------- physical page API (wraps buddy) ----------------
uintptr_t mm_alloc_pages(uint8_t order) {
	return buddy_alloc(order);
}

void mm_free_pages(uintptr_t pa, uint8_t order) {
	buddy_free(pa, order);
}

// ---------------- heap API (wraps slob) ----------------
void mm_heap_init(void) {
	slob_init();
}

void *kmalloc(size_t size) {
	return slob_alloc(size);
}

void *kzalloc(size_t size) {
	void *p = kmalloc(size);

	if (p) {
		memset(p, 0, size);
	}

	return p;
}

void kfree(void *ptr) {
	slob_free(ptr);
}

// ---------------- HHDM helpers ----------------
void *hhdm_map_pa(uintptr_t pa) {
	return (void *)(pa + HHDM_OFFSET);
}

uintptr_t hhdm_to_pa(void *va) {
	return (uintptr_t)va - HHDM_OFFSET;
}
