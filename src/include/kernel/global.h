#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uintptr_t paddr_t;
typedef uint64_t vaddr_t;

#pragma pack(push, 1)

typedef struct {
	uint16_t isr_low;		// Lower 16 bits of ISR's addr
	uint16_t kernel_cs; // GDT selector for kernel (0x08)
	uint8_t ist;				// Interrupt Stack Table offset (0 if unused)
	uint8_t attributes; // Type and attributes
	uint16_t isr_mid;		// Middle 16 bits of ISR's addr
	uint32_t isr_high;	// Higher 32 bits of ISR's addr
	uint32_t reserved;	// Set to 0
} idt_entry_t;

typedef struct {
	uint16_t limit;
	uint64_t base;
} idtptr_t;

// Structure for a single 64-bit GDT
typedef struct {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middl;
	uint8_t access;
	uint8_t flags_limit_hi;
	uint8_t base_high;
} gdt_entry_t;

//	Structure to pass to the LGDT instruction
typedef struct {
	uint16_t limit;
	uint64_t base;
} gdtptr_t;

#pragma pack(pop)

// Clone of EFI_GRAPHICS_PIXEL_FORMAT enums
typedef enum {
	PX_RGBRESV_8BIT_PER_COLOR,
	PX_BGRRESV_8BIT_PER_COLOR,
	PX_BIT_MASK,
	PX_BLT_ONLY,
	PX_FORMAT_MAX
} px_format_t;

typedef struct {
	uint32_t *base_addr;			// Pointer to the start of video memory
	size_t size;							// Total size of the framebuffer in bytes
	uint32_t width;						// Horizontal resolution (e.g., 1920)
	uint32_t height;					// Vertical resolution (e.g., 1080)
	uint32_t px_per_scanline; // The actual row width in memory
	px_format_t px_format;		// Enum of pixel format
} framebuffer_t;

typedef struct {
	uint32_t type; // Field size is 32 bits followed by 32 bit pad
	uint32_t pad;
	paddr_t pstart; // Field size is 64 bits
	vaddr_t vstart; // Field size is 64 bits
	size_t num_pg;	// Field size is 64 bits
	uint64_t attr;	// Field size is 64 bits
} mem_descriptor_t;

typedef struct {
	mem_descriptor_t *mdesc;
	size_t msz;
	size_t dsz;
} mmap_t;

typedef struct {
	mem_descriptor_t *map;
	size_t map_size;
	size_t desc_size;
} memmap_t;

typedef struct {
	paddr_t pstart;
	paddr_t pend;
} allocated_t;

// Boot info passed to kernel after ExitBootServices
typedef struct {
	uint32_t flags; // Bitmask of boot info flags
	// framebuffer
	framebuffer_t frame_buff;
	// memory map
	memmap_t mem_map;
	size_t total_installed_ram;
	paddr_t highest_phys_addr;
	paddr_t pml4_paddr;
	paddr_t kernel_phys_start;
	paddr_t kstack_base;
	allocated_t alloc_pages[20];
} bootinfo_t;

// Define our GDT with 5 entries
extern gdt_entry_t _gdt[5];
extern gdtptr_t _gdt_ptr;

extern idt_entry_t _idt[256];
extern idtptr_t _idt_ptr;

extern bootinfo_t _bi;
