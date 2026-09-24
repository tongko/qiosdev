#include <kernel/mm.h>
#include <kernel/buddy.h>
#include <kernel/global.h>
#include <kernel/slob.h>
#include <kernel/klog.h>
#include <libk/string.h>

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

bool mm_map_virt_addr(uint64_t *pml4, vaddr_t vaddr, paddr_t paddr, uint64_t flags, size_t sz, page_type_t type) {
	if (!pml4 || !sz || (type == PAGE_2M && (vaddr % PAGE_HUGE_SIZE || paddr % PAGE_HUGE_SIZE)) ||
		 (type == PAGE_4K && (vaddr % PAGE_SIZE || paddr % PAGE_SIZE))) {
		printk("mm_map_virt_addr: Invalid parameter. pml4=%llx, v=%llx, p=%llx, f=%llx, sz=%llu, t=%d",
				 pml4,
				 vaddr,
				 paddr,
				 flags,
				 sz,
				 type);
		return false;
	}

	size_t pg_sz = type == PAGE_2M ? PAGE_HUGE_SIZE : PAGE_SIZE;
	// Align size to the next full page boundry
	size_t num_pg = (sz + pg_sz - 1) / pg_sz;

	for (size_t i = 0; i < num_pg; i++) {
		vaddr_t cur_vaddr = vaddr + (i * pg_sz);
		paddr_t cur_paddr = paddr + (i * pg_sz);

		// Extract indexes
		size_t pml4_idx = PML4_IDX(cur_vaddr);
		size_t pdpt_idx = PDPT_IDX(cur_vaddr);
		size_t pd_idx = PD_IDX(cur_vaddr);

		if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
			uint64_t *new_pdpt = (uint64_t *)mm_alloc_pages(0);
			if (!new_pdpt) {
				printk("mm_map_virt_addr: PDPT allocate page failed.");
				return false;
			}

			pml4[pml4_idx] = (uint64_t)new_pdpt | PAGE_WRITE | PAGE_PRESENT;
		}
		uint64_t *pdpt = (uint64_t *)(pml4[pml4_idx] & ADDRESS_MASK);

		if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
			uint64_t *new_pd = (uint64_t *)mm_alloc_pages(0);
			if (!new_pd) {
				printk("mm_map_virt_addr: PD allocate page failed.");
				return false;
			}
			pdpt[pdpt_idx] = (uint64_t)new_pd | PAGE_WRITE | PAGE_PRESENT;
		}
		uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & ADDRESS_MASK);

		if (type == PAGE_2M) {
			pd[pd_idx] = cur_paddr | flags | PAGE_HUGE | PAGE_WRITE | PAGE_PRESENT;
			continue;
		}

		if (!(pd[pd_idx] & PAGE_PRESENT)) {
			uint64_t *new_pt = (uint64_t *)mm_alloc_pages(0);
			if (!new_pt) {
				printk("mm_map_virt_addr: PT allocate page failed.");
				return false;
			}
			pd[pd_idx] = (uint64_t)new_pt | PAGE_WRITE | PAGE_PRESENT;
		}
		uint64_t *pt = (uint64_t *)(pd[pd_idx] & ADDRESS_MASK);

		pt[PT_IDX(cur_vaddr)] = cur_paddr | flags | PAGE_WRITE | PAGE_PRESENT;
	}

	return true;
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
