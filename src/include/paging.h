#pragma once

#include <efi.h>
#include <bootinfo.h>

#define READ_CR3                                                               \
	({                                                                           \
		UINTN cr3;                                                                 \
		__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));                             \
		cr3;                                                                       \
	})

#define SET_CR3(pml) __asm__ volatile("mov %0, %%cr3" : : "r"(pml) : "memory");

#define HHDM_OFFSET 0xFFFF800000000000ULL

#define ENTRY_MASK 0x1ff
#define ADDRESS_MASK ~0xfffUL

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
#define PAGE_UNCACHEABLE                                                       \
	(PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT | PAGE_HUGE)

typedef enum { PAGE_4K, PAGE_2M } page_type_t;

EFI_STATUS paging_init(IN bootinfo_t *bi);

EFI_STATUS hhdm_init(EFI_PHYSICAL_ADDRESS highest_paddr);

EFI_STATUS map_virt_addr(EFI_VIRTUAL_ADDRESS vaddr, EFI_PHYSICAL_ADDRESS paddr,
												 UINT64 flags, UINTN sz, page_type_t type);

bool is_paging_init();
