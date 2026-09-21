#pragma once

#include <efi.h>

#define ELF_MAGIC 0x464C457FU
#define PT_LOAD 1
// p_flags constants
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

typedef struct {
	UINT8 e_ident[16];			  // ELF Identification bytes (Magic number)
	UINT16 e_type;					  // Object file type (Executable, Shared, etc.)
	UINT16 e_machine;				  // Architecture (e.g., x86_64 = 0x3E)
	UINT32 e_version;				  // Object file version
	EFI_VIRTUAL_ADDRESS e_entry; // Virtual entry point address (Kernel Main)
	UINT64 e_phoff;				  // Program header table file offset
	UINT64 e_shoff;				  // Section header table file offset
	UINT32 e_flags;				  // Processor-specific flags
	UINT16 e_ehsize;				  // ELF header size in bytes
	UINT16 e_phentsize;			  // Size of each program header entry
	UINT16 e_phnum;				  // Number of program header entries
	UINT16 e_shentsize;			  // Size of one section header table entry
	UINT16 e_shnum;				  // Number of section header table entries
	UINT16 e_shstrndx;			  // Section header string table index
} elf64_ehdr_t;

typedef struct {
	UINT32 p_type;						// Segment type (e.g., PT_LOAD = 1)
	UINT32 p_flags;					// Segment flags (Execute=1, Write=2, Read=4)
	UINTN p_offset;					// File offset where the segment data starts
	EFI_VIRTUAL_ADDRESS p_vaddr;	// Virtual address to load the segment into
	EFI_PHYSICAL_ADDRESS p_paddr; // Physical address (NOT IN USE)
	UINTN p_filesz;					// Segment size inside the ELF file
	UINTN p_memsz;						// Segment size inside active RAM (can be larger)
	UINT64 p_align;					// Segment alignment boundary requirements
} elf64_phdr_t;

EFI_STATUS open_file(IN const CHAR16 *fname, OUT EFI_FILE_PROTOCOL **out_file);

EFI_STATUS load_elf(IN const EFI_FILE_HANDLE hfile, OUT UINTN *out_entry);

EFI_STATUS load_logo(IN const EFI_FILE_HANDLE hfile, VOID **out_buf, OUT UINTN *out_sz);