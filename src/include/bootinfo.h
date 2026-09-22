#pragma once

#include <efi.h>

typedef struct {
	UINT32 *base_addr;						 // Pointer to the start of video memory
	UINTN size;									 // Total size of the framebuffer in bytes
	UINT32 width;								 // Horizontal resolution (e.g., 1920)
	UINT32 height;								 // Vertical resolution (e.g., 1080)
	UINT32 px_per_scanline;					 // The actual row width in memory
	EFI_GRAPHICS_PIXEL_FORMAT px_format; // Enum of pixel format
} framebuffer_t;

typedef struct {
	EFI_MEMORY_DESCRIPTOR *map;
	UINTN map_size;
	UINTN desc_size;
} memmap_t;

typedef struct {
	EFI_PHYSICAL_ADDRESS pstart;
	EFI_PHYSICAL_ADDRESS pend;
} allocated_t;

// Boot info passed to kernel after ExitBootServices
typedef struct {
	UINT32 flags; // Bitmask of boot info flags
	UINTN tsc_freq_hz;
	UINTN tsc_start;
	// framebuffer
	framebuffer_t frame_buff;
	// memory map
	memmap_t mem_map;
	UINTN total_installed_ram;
	EFI_PHYSICAL_ADDRESS highest_phys_addr;
	EFI_PHYSICAL_ADDRESS pml4_paddr;
	EFI_PHYSICAL_ADDRESS kernel_phys_start;
	EFI_PHYSICAL_ADDRESS kstack_base;
	EFI_VIRTUAL_ADDRESS logo_bmp[10];
	allocated_t alloc_pages[20];
} bootinfo_t;

EFI_STATUS bootinfo_init(bootinfo_t *bi);

void reorder_alloc(bootinfo_t *bi);