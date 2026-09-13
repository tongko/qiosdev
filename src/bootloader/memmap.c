#include <memmap.h>
#include <bootinfo.h>
#include <efilib.h>

#define IS_RAM_TYPE(x) ((x > 0 && x < 7) || (x > 8 && x < 10) || x == 14)

#if defined(EFI_DEBUG) && defined(EFI_DEBUG_MEM)
static CHAR16 _temp_buff[100] = {0};

/*****************************************************************************
 * function: get_type_str
 * Convert EFI_MEMORY_TYPE to string representation
 ******************************************************************************/
static CHAR16 *get_type_str(EFI_MEMORY_TYPE type) {
	switch (type) {
	case EfiReservedMemoryType:
		return u"Reserved";
	case EfiLoaderCode:
		return u"LoaderCode";
	case EfiLoaderData:
		return u"LoaderData";
	case EfiBootServicesCode:
		return u"BootServicesCode";
	case EfiBootServicesData:
		return u"BootServicesData";
	case EfiRuntimeServicesCode:
		return u"RuntimeServicesCode";
	case EfiRuntimeServicesData:
		return u"RuntimeServicesData";
	case EfiConventionalMemory:
		return u"ConventionalMemory";
	case EfiUnusableMemory:
		return u"UnusableMemory";
	case EfiACPIReclaimMemory:
		return u"ACPIReclaimMemory";
	case EfiACPIMemoryNVS:
		return u"ACPIMemoryNVS";
	case EfiMemoryMappedIO:
		return u"MemoryMappedIO";
	case EfiMemoryMappedIOPortSpace:
		return u"MemoryMappedIOPortSpace";
	case EfiPalCode:
		return u"PalCode";
	case EfiPersistentMemory:
		return u"PersistentMemory";
	case EfiUnacceptedMemoryType:
		return u"UnacceptedMemoryType";
	default:
		return u"MaxMemoryType(?)";
	}
}

/*****************************************************************************
 * function: get_attr_str
 * Convert memory attribute to string representation
 ******************************************************************************/
static CHAR16 *get_attr_str(UINTN attr) {
	ZeroMem(_temp_buff, sizeof(_temp_buff));

	if (attr & EFI_MEMORY_UC) {
		StrCat(_temp_buff, u"UC ");
	}
	if (attr & EFI_MEMORY_WC) {
		StrCat(_temp_buff, u"WC ");
	}
	if (attr & EFI_MEMORY_WT) {
		StrCat(_temp_buff, u"WT ");
	}
	if (attr & EFI_MEMORY_WB) {
		StrCat(_temp_buff, u"WB ");
	}
	if (attr & EFI_MEMORY_UCE) {
		StrCat(_temp_buff, u"UCE ");
	}
	if (attr & EFI_MEMORY_WP) {
		StrCat(_temp_buff, u"WP ");
	}
	if (attr & EFI_MEMORY_RP) {
		StrCat(_temp_buff, u"RP ");
	}
	if (attr & EFI_MEMORY_XP) {
		StrCat(_temp_buff, u"XP ");
	}
	if (attr & EFI_MEMORY_RUNTIME) {
		StrCat(_temp_buff, u"RUNTIME ");
	}

	return _temp_buff;
}
#endif

/*****************************************************************************
 * function: mem_init
 * Initialize memory
 ******************************************************************************/
EFI_STATUS mem_init(bootinfo_t *bi) {
	if (!bi) {
		Print(u"%E❌ [mem_init] Invalid parameter 'bi'.%N\r\n");
		return EFI_INVALID_PARAMETER;
	}

	// Let's first get memory map for further tasks.
	EFI_MEMORY_DESCRIPTOR *map;
	UINTN map_sz;
	UINTN desc_sz;
	UINTN key;
	EFI_STATUS status = get_memmap(&map, &map_sz, &desc_sz, &key);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [mem_init] Out of resources.%N\r\n");
		return EFI_OUT_OF_RESOURCES;
	}

	bi->mem_map.map = map;
	bi->mem_map.map_size = map_sz;
	bi->mem_map.desc_size = desc_sz;

	// Probe all non-reserved memory using the memory map
	EFI_PHYSICAL_ADDRESS haddr = 0;
	status = get_non_resv_mem(&(bi->mem_map), &haddr);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [mem_init] Get non reserved mem failed: %r%N\r\n", status);
		return EFI_OUT_OF_RESOURCES;
	}

	bi->highest_phys_addr = haddr;

	return status;
}

/*****************************************************************************
 * function: get_memmap
 * Get memory map descriptor from UEFI
 ******************************************************************************/
EFI_STATUS get_memmap(OUT EFI_MEMORY_DESCRIPTOR **out_map, OUT UINTN *map_sz,
											OUT UINTN *desc_sz, OUT UINTN *key) {
	EFI_STATUS status;
	UINTN msz = 0, dsz = 0;
	EFI_MEMORY_DESCRIPTOR *map = NULL;
	UINT64 k = 0;
	UINT32 dv = 0;

	// This is MS style for enumerating available items, first call the function
	// with 0 size to get the exact buffer size needed by the whole item
	// collection
	status = BS->GetMemoryMap(&msz, NULL, &k, &dsz, &dv);
	// expecting it to return EFI_BUFFER_TOO_SMALL
	if (status != EFI_BUFFER_TOO_SMALL) {
		Print(u"%E❌ [mem_init] Failed to get buffer size: %r.%N\r\n", status);
	}
	// Because get_memmap will allocate 2 pages, so we add padding to the msz
	msz += (2 * dsz);
	map = AllocateZeroPool(msz);
	if (!map) {
		FreePool(map);
		Print(u"%E❌ [mem_init] Out of resources.%N\r\n");
		return EFI_OUT_OF_RESOURCES;
	}

	// Call GetMemoryMap second time to populate our buff
	status = BS->GetMemoryMap(&msz, map, &k, &dsz, &dv);
	if (EFI_ERROR(status)) {
		FreePool(map);
		Print(u"❌ [get_memmap]: Get memory map failed: %r%N\r\n", status);
		return status;
	}

	// Pass the populated values back to caller.
	*out_map = map;
	*map_sz = msz;
	*desc_sz = dsz;
	*key = k;

#if defined(EFI_DEBUG) && defined(EFI_DEBUG_MEM)
	Print(u"[DEBUG][get_memmap] Memory map init successfully.\r\n");
	Print(u"[DEBUF][get_memmap] ✅ Address         : 0x%lx\r\n", map);
	Print(u"[DEBUF][get_memmap] ✅ Map size        : %lu\r\n", *map_sz);
	Print(u"[DEBUF][get_memmap] ✅ Descriptor size : %lu\r\n", *desc_sz);
#endif

	return status;
}

/*****************************************************************************
 * function: get_non_resv_mem
 * Calculate the highest non-reserved-memory for HHDM
 ******************************************************************************/
EFI_STATUS get_non_resv_mem(IN memmap_t *mem_map,
														OUT EFI_PHYSICAL_ADDRESS *highest_addr) {
	if (!(mem_map && highest_addr)) {
		return EFI_INVALID_PARAMETER;
	}

	EFI_MEMORY_DESCRIPTOR *map = mem_map->map;
	UINTN msz = mem_map->map_size;
	UINTN dsz = mem_map->desc_size;

	if (!(map && msz && dsz)) {
		return EFI_INVALID_PARAMETER;
	}

	UINTN num_e = msz / dsz;
	UINTN haddr = 0;

#if defined(EFI_DEBUG) && defined(EFI_DEBUG_MEM)
	Print(u"[DEBUG][get_non_resv_mem] Mem map addr: 0x%lx\r\n", map);
	Print(u"[DEBUG][get_non_resv_mem] List mem map:\r\n");
	EFI_PHYSICAL_ADDRESS prev = 0;
#endif
	for (UINTN i = 0; i < num_e; i++) {
		EFI_MEMORY_DESCRIPTOR *desc =
				(EFI_MEMORY_DESCRIPTOR *)((UINT8 *)map + (i * dsz));
#if defined EFI_DEBUG && defined(EFI_DEBUG_MEM)
		Print(u"[DEBUG][get_non_resv_mem]\tDescriptor %d - ", i);
		if (prev != desc->PhysicalStart) {
			Print(u"❌\r\n");
		} else {
			Print(u"✅\r\n");
		}
		Print(u"[DEBUG][get_non_resv_mem]\t\tType       : %s\r\n",
					get_type_str(desc->Type));
		Print(u"[DEBUG][get_non_resv_mem]\t\tVirt start : 0x%lx\r\n",
					desc->VirtualStart);
		Print(u"[DEBUG][get_non_resv_mem]\t\tPhys start : 0x%lx\r\n",
					desc->PhysicalStart);
		Print(u"[DEBUG][get_non_resv_mem]\t\tPadding    : %d\r\n", desc->Pad);
		Print(u"[DEBUG][get_non_resv_mem]\t\tNum Pages  : %lu\r\n",
					desc->NumberOfPages);
		Print(u"[DEBUG][get_non_resv_mem]\t\tAttributes : %s\r\n",
					get_attr_str(desc->Attribute));
		prev = (desc->PhysicalStart + (desc->NumberOfPages * 4096));
		Print(u"[DEBUG][get_non_resv_mem]\t\tPhys end   : 0x%lx\r\n", prev);
#endif
		if (IS_RAM_TYPE(desc->Type)) {
			EFI_PHYSICAL_ADDRESS paddr =
					desc->PhysicalStart + (desc->NumberOfPages * EFI_PAGE_SIZE);
			if (paddr > haddr) {
				haddr = paddr;
			}
		}
	}

	*highest_addr = haddr;
	return EFI_SUCCESS;
}

EFI_PHYSICAL_ADDRESS alloc_pages(EFI_ALLOCATE_TYPE type_alloc,
																 EFI_MEMORY_TYPE type_mem, UINTN num_pg) {
	EFI_PHYSICAL_ADDRESS paddr = 0;
	EFI_STATUS status = BS->AllocatePages(type_alloc, type_mem, num_pg, &paddr);
	if (EFI_ERROR(status)) {
		Print(u"%E❌ [alloc_zero_pages] Allocte pages failed: %r%N\r\n", status);
		return 0;
	}

	BS->SetMem((VOID *)paddr, num_pg * EFI_PAGE_SIZE, 0);

	return paddr;
}