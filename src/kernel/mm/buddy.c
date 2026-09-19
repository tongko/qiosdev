#include <kernel/buddy.h>
#include <kernel/global.h>
#include <libk/string.h>
#include <stdbool.h>
#include <stddef.h>

// free_list[order] is head of linked list for blocks of 2^order pages
static page_t *_free_list[MAX_ORDER + 1];
static page_t *_page_array; // base of global page metadata array
static uintptr_t _page_array_pa_base;
static size_t _total_page_count;

// Should reclaim the memory?
static inline bool is_ramtype(mem_descriptor_t *d) {
	if (!d) {
		return false;
	}

	uint32_t x = d->type;
	// Type 14 is special case, we don't deal with it now
	// We will not call UEFI anymore, thus Type 5 and 6
	// can be reclaim
	return ((x > 0 && x <= 7) || (x > 8 && x < 10));
}

// Given page index and order, return buddy index
static inline size_t buddy_idx(size_t idx, int order) {
	return idx ^ (1ULL << order);
}

// Check if buddy is free and same order
static bool buddy_is_free(size_t idx, int order) {
	size_t bidx = buddy_idx(idx, order);
	if (bidx >= _total_page_count)
		return false;
	struct page *bp = &_page_array[bidx];
	return bp->is_free && (bp->order == order);
}

static void list_add(struct page **head, struct page *p) {
	p->next = *head;
	*head = p;
}

static void list_remove(struct page **head, struct page *target) {
	if (*head == target) {
		*head = target->next;
		return;
	}
	struct page *cur = *head;
	while (cur && cur->next != target) {
		cur = cur->next;
	}
	if (cur) {
		cur->next = target->next;
	}
}

// returns physical address, 0 on failure
uintptr_t buddy_alloc(int req_order) {
	if (req_order < 0 || req_order > MAX_ORDER)
		return 0;

	// Find smallest free block >= req_order
	int order;
	for (order = req_order; order <= MAX_ORDER; order++) {
		if (_free_list[order])
			break;
	}
	if (order > MAX_ORDER)
		return 0; // no memory

	// Pop block from free list
	struct page *block = _free_list[order];
	list_remove(&_free_list[order], block);
	block->is_free = false;

	// Split down to requested order
	while (order > req_order) {
		order--;
		size_t block_idx = block - _page_array;
		size_t buddy = buddy_idx(block_idx, order);
		struct page *buddy_page = &_page_array[buddy];

		buddy_page->is_free = true;
		buddy_page->order = order;
		list_add(&_free_list[order], buddy_page);
	}

	uintptr_t pa = idx_to_pa(block - _page_array);
	return pa;
}

void buddy_free(uintptr_t pa, int order) {
	size_t idx = pa_to_idx(pa);
	struct page *p = &_page_array[idx];
	p->is_free = true;
	p->order = order;

	// Coalesce upwards as much as possible
	while (order < MAX_ORDER) {
		if (!buddy_is_free(idx, order))
			break;

		// Remove buddy from free list
		size_t bidx = buddy_idx(idx, order);
		struct page *bp = &_page_array[bidx];
		list_remove(&_free_list[order], bp);

		// merged block starts at smaller index
		if (bidx < idx) {
			idx = bidx;
		}
		order++;
	}
	p = &_page_array[idx];
	p->is_free = true;
	p->order = order;
	list_add(&_free_list[order], p);
}

void buddy_add_range(uintptr_t start_pa, uintptr_t end_pa) {
	size_t start_idx = pa_to_idx(start_pa);
	size_t end_idx = pa_to_idx(end_pa);
	size_t idx = start_idx;

	while (idx < end_idx) {
		// Find largest possible order for current idx
		int order = 0;
		while (order < MAX_ORDER) {
			size_t next_idx = idx + (1ULL << (order + 1));
			if (next_idx > end_idx)
				break;
			if (buddy_idx(idx, order + 1) >= _total_page_count)
				break;
			order++;
		}

		page_t *p = &_page_array[idx];
		p->is_free = true;
		p->order = order;
		list_add(&_free_list[order], p);

		idx += (1ULL << order);
	}
}

void buddy_init(mem_descriptor_t *map, size_t msz, size_t dsz) {
	// Scan memory map to find max physical address
	uintptr_t max_pa = 0;
	for (size_t off = 0; off < msz; off += dsz) {
		mem_descriptor_t *d = (void *)((uintptr_t)map + off);
		if (!is_ramtype(d)) {
			continue;
		}
		uintptr_t end = d->pstart + (d->num_pg * PAGE_SIZE);
		if (end > max_pa) {
			max_pa = end;
		}
	}
	_total_page_count = max_pa / PAGE_SIZE;

	// Allocate _page_array from EfiConventionalMemory
	size_t page_array_bytes = _total_page_count * sizeof(page_t);
	// Find a free physical region large enough for _page_array, mark it as used.
	// First ever fit.
	for (size_t off = 0; off < msz; off += dsz) {
		mem_descriptor_t *d = (void *)((uintptr_t)map + off);
		if (!is_ramtype(0)) {
			continue;
		}
		size_t sz = d->num_pg * PAGE_SIZE;
		if (sz > page_array_bytes) {
			// OK, large enough, pick this
			_page_array_pa_base = d->pstart;
			// Reset this descriptor
			d->pstart += sz;
			d->vstart += sz;
			break;
		}
	}

	// (implement your own _page_array allocation here)
	// _page_array_pa = allocated physical address
	_page_array = HHDM(_page_array_pa_base);

	// Zero all page structs
	memset(_page_array, 0, page_array_bytes);

	// Init _free_list to NULL
	for (int i = 0; i <= MAX_ORDER; i++) {
		_free_list[i] = NULL;
	}

	// Walk memory map again, add all reclaimable memory
	for (size_t off = 0; off < msz; off += dsz) {
		mem_descriptor_t *d = (void *)((uintptr_t)map + off);
		if (!is_ramtype(d)) {
			continue;
		}

		uintptr_t s = d->pstart;
		uintptr_t e = s + d->num_pg * PAGE_SIZE;
		size_t i = 0;
		while (i < 20) {
			allocated_t a = _bi.alloc_pages[i];
			if (a.pstart == s) {
				// skip a
				s = a.pend;
			} else if (a.pstart >= s && a.pstart < e) {
				// a is part of d;
				buddy_add_range(s, a.pstart);
				s = a.pend;
			}
		}

		if (s < e) {
			buddy_add_range(s, e);
		}
	}
}