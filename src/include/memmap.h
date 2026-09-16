#pragma once

#include <bootinfo.h>
#include <efi.h>

EFI_STATUS mem_init(bootinfo_t *bi);

EFI_STATUS get_memmap(OUT EFI_MEMORY_DESCRIPTOR **out_map, OUT UINTN *map_sz,
											OUT UINTN *desc_sz, OUT UINTN *key);

EFI_STATUS get_non_resv_mem(IN memmap_t *mem_map,
														OUT EFI_PHYSICAL_ADDRESS *highest_addr);

EFI_PHYSICAL_ADDRESS alloc_pages(EFI_ALLOCATE_TYPE type_alloc,
																 EFI_MEMORY_TYPE type_mem, UINTN num_pg);

void free_pages(EFI_PHYSICAL_ADDRESS paddr, UINTN numpg);