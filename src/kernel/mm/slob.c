#include <kernel/buddy.h>
#include <kernel/slob.h>
#include <stddef.h>
#include <stdint.h>

slob_page_t *_slob_page_list = NULL;
#define SLOB_PAGE_SIZE 4096

static slob_page_t *slob_allocate_new_page(void) {
	// Ask buddy allocator for one 4K physical page frame (order 0)
	uintptr_t pa = buddy_alloc(0);
	if (!pa) {
		return NULL;
	}

	// Convert physical address to kernel virtual address (HHDM)
	void *va = HHDM(pa);
	slob_page_t *sp = (slob_page_t *)va;

	sp->next_page = NULL;
	sp->used_bytes = sizeof(slob_page_t);

	// The free region starts right after slob_page header
	size_t free_region_start = sizeof(slob_page_t);
	slob_block_t *blk = (slob_block_t *)((uint8_t *)va + free_region_start);

	blk->size = SLOB_PAGE_SIZE - free_region_start - sizeof(slob_block_t);
	blk->next = NULL;
	sp->free_list = blk;

	// Append new slob page to global page list
	sp->next_page = _slob_page_list;
	_slob_page_list = sp;
	return sp;
}

void *slob_alloc(size_t size) {
	// Align to 8 bytes (x86_64 requirement)
	size = (size + 7) & ~7;
	if (size == 0) {
		size = 8;
	}

	// Scan existing slob pages, find first free block that fits (first-fit)
	slob_page_t *p = _slob_page_list;
	for (; p; p = p->next_page) {
		slob_block_t **prev_ptr = &p->free_list;
		slob_block_t *blk = p->free_list;
		while (blk) {
			if (blk->size >= size) {
				// Split block if leftover space is enough for new slob_block header
				size_t leftover = blk->size - size;
				if (leftover >= sizeof(slob_block_t) + 8) {
					slob_block_t *new_blk =
							(slob_block_t *)((uint8_t *)blk + sizeof(slob_block_t) + size);
					new_blk->size = leftover - sizeof(slob_block_t);
					new_blk->next = blk->next;

					blk->size = size;
					*prev_ptr = new_blk;
				} else {
					// use entire block, remove from free list
					*prev_ptr = blk->next;
				}

				p->used_bytes += size;
				// return payload address (skip block header)
				return (void *)((uint8_t *)blk + sizeof(slob_block_t));
			}
			prev_ptr = &blk->next;
			blk = blk->next;
		}
	}

	// 2. No free block found: allocate new page from buddy
	slob_page_t *newpage = slob_allocate_new_page();
	if (!newpage) {
		return NULL;
	}

	// Recursive call to allocate from newly created page
	return slob_alloc(size);
}

void slob_free(void *ptr) {
	if (!ptr) {
		return;
	}

	// get back block header: subtract header size
	slob_block_t *blk = (slob_block_t *)((uint8_t *)ptr - sizeof(slob_block_t));

	// Find which slob_page this block belongs to
	slob_page_t *page = _slob_page_list;
	for (; page; page = page->next_page) {
		uintptr_t page_start = (uintptr_t)page;
		uintptr_t blk_addr = (uintptr_t)blk;
		if (blk_addr >= page_start && blk_addr < page_start + SLOB_PAGE_SIZE) {
			break;
		}
	}

	if (!page) {
		return; // invalid pointer, ignore
	}

	// Add block back to page free list (simple prepend)
	blk->next = page->free_list;
	page->free_list = blk;
	page->used_bytes -= blk->size;

	// Check: if page is fully empty, return to buddy allocator
	if (page->used_bytes == sizeof(slob_page_t)) {
		// remove page from global slob page list
		slob_page_t **pp = &_slob_page_list;
		while (*pp && *pp != page) {
			pp = &(*pp)->next_page;
		}

		if (*pp) {
			*pp = page->next_page;
		}

		// convert VA back to PA, return page frame to buddy
		uintptr_t pa = HHDM_INV((uintptr_t)page);
		buddy_free(pa, 0);
	}
}

void slob_init(void) { _slob_page_list = NULL; }
