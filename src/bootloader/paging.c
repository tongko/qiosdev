#include <memmap.h>
#include <paging.h>
#include <efi.h>
#include <efilib.h>

static UINT64 *_pml4_table;

bool is_paging_init() { return _pml4_table != NULL; }

/*****************************************************************************
 * function: paging_init
 * Initialize paging
 ******************************************************************************/
EFI_STATUS paging_init(IN OUT bootinfo_t *bi) {
	EFI_STATUS status;

	// Get actice PML4 address from CR3
	UINT64 *pml4 = (UINT64 *)(READ_CR3 & ADDRESS_MASK);
	// Allocate a page frame for the new PML4 table
	_pml4_table =
			(UINT64 *)alloc_pages(AllocateAnyPages, EfiRuntimeServicesData, 1);
	if (!_pml4_table) {
		Print(u"%E❌ [paging_init] PML4 allocate page failed.\r\n");
		return EFI_OUT_OF_RESOURCES;
	}

	Print(u"[paging_init] PML4 table allocated to 0x%lx\r\n", _pml4_table);
	CopyMem((VOID *)_pml4_table, (VOID *)pml4, EFI_PAGE_SIZE);

	bi->pml4_paddr = (EFI_PHYSICAL_ADDRESS)_pml4_table;

	status = hhdm_init(bi->highest_phys_addr);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [paging_init] Mapping HHDM failed: %r%N\r\n", status);
	}

	return status;
}

/*****************************************************************************
 * function: hhdm_init
 * Map HHDM to _pml4_table
 ******************************************************************************/
EFI_STATUS hhdm_init(IN EFI_PHYSICAL_ADDRESS highest_paddr) {
	if (!highest_paddr) {
		return EFI_INVALID_PARAMETER;
	}

	if (!_pml4_table) {
		Print(u"%E❌ [hhdm_init] Paging not initialize.\r\n");
		return EFI_NOT_READY;
	}

	Print(u"[hhdm_init] Mapping 0x0 - 0x%lx as HHDM.\r\n", highest_paddr);
	EFI_STATUS status =
			map_virt_addr(HHDM_OFFSET, 0, PAGE_WRITE_BACK, highest_paddr, PAGE_2M);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [hhdm_init] Mapping HHDM virtual address failed: %r%N\r\n",
					status);
		return status;
	}

	return status;
}

/*****************************************************************************
 * function: map_virt_addr
 * Map virtual address to physical address in _pml4_table
 ******************************************************************************/
EFI_STATUS map_virt_addr(EFI_VIRTUAL_ADDRESS vaddr, EFI_PHYSICAL_ADDRESS paddr,
												 UINT64 flags, UINTN sz, page_type_t type) {
	if (!sz ||
			(type == PAGE_2M && (vaddr % PAGE_HUGE_SIZE || paddr % PAGE_HUGE_SIZE)) ||
			(type == PAGE_4K && (vaddr % EFI_PAGE_SIZE || paddr % EFI_PAGE_SIZE))) {
		Print(u"%E❌ [map_virt_addr] Invalid parameters.%N\r\n");
		return EFI_INVALID_PARAMETER;
	}

	if (!_pml4_table) {
		Print(u"%E❌ [map_virt_addr] Paging not initialize.\r\n");
		return EFI_NOT_READY;
	}

	UINTN pg_sz = type == PAGE_2M ? PAGE_HUGE_SIZE : EFI_PAGE_SIZE;

	// Align size to the next full page boundry
	UINTN num_pg = (sz + pg_sz - 1) / pg_sz;

	for (UINTN i = 0; i < num_pg; i++) {
		EFI_VIRTUAL_ADDRESS cur_vaddr = vaddr + (i * pg_sz);
		EFI_PHYSICAL_ADDRESS cur_paddr = paddr + (i * pg_sz);

		// Extract indexes
		UINTN pml4_idx = PML4_IDX(cur_vaddr);
		UINTN pdpt_idx = PDPT_IDX(cur_vaddr);
		UINTN pd_idx = PD_IDX(cur_vaddr);

		if (!(_pml4_table[pml4_idx] & PAGE_PRESENT)) {
			UINT64 *new_pdpt =
					(UINT64 *)alloc_pages(AllocateAnyPages, EfiRuntimeServicesData, 1);
			if (!new_pdpt) {
				Print(u"%E❌ [map_virt_addr] PDPT allocate page failed.\r\n");
				return EFI_OUT_OF_RESOURCES;
			}

			_pml4_table[pml4_idx] = (UINT64)new_pdpt | PAGE_WRITE | PAGE_PRESENT;
		}
		UINT64 *pdpt = (UINT64 *)(_pml4_table[pml4_idx] & ADDRESS_MASK);

		if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
			UINT64 *new_pd =
					(UINT64 *)alloc_pages(AllocateAnyPages, EfiRuntimeServicesData, 1);
			if (!new_pd) {
				Print(u"%E❌ [map_virt_addr] PD allocate page failed.\r\n");
				return EFI_OUT_OF_RESOURCES;
			}
			pdpt[pdpt_idx] = (UINT64)new_pd | PAGE_WRITE | PAGE_PRESENT;
		}
		UINT64 *pd = (UINT64 *)(pdpt[pdpt_idx] & ADDRESS_MASK);

		if (type == PAGE_2M) {
			pd[pd_idx] = cur_paddr | flags | PAGE_HUGE | PAGE_WRITE | PAGE_PRESENT;
			continue;
		}

		if (!(pd[pd_idx] & PAGE_PRESENT)) {
			UINT64 *new_pt =
					(UINT64 *)alloc_pages(AllocateAnyPages, EfiRuntimeServicesData, 1);
			if (!new_pt) {
				Print(u"%E❌ [map_virt_addr] PT allocate page failed.\r\n");
				return EFI_OUT_OF_RESOURCES;
			}
			pd[pd_idx] = (UINT64)new_pt | PAGE_WRITE | PAGE_PRESENT;
		}
		UINT64 *pt = (UINT64 *)(pd[pd_idx] & ADDRESS_MASK);

		pt[PT_IDX(cur_vaddr)] = cur_paddr | flags | PAGE_WRITE | PAGE_PRESENT;
	}

	return EFI_SUCCESS;
}
