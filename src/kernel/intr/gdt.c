#include <kernel/gdt.h>
#include <stdint.h>

gdt_entry_t _gdt[5];
gdtptr_t _gdt_ptr;

void gdt_init() {
	// 0x00: Null descriptor
	_gdt[0] = (gdt_entry_t){0, 0, 0, 0, 0, 0};

	// 0x08: Kernel Code Segment
	// Access: Present(1) Ring0(00) System(1) Executable(1) Conforming(0)
	// 		Readable(1) Accessed(0) -> 0x9A
	// Flags: Granularity(1) Size64(1) Size32(0) -> 0x20 (place in high 4 bit of
	// 		flags_limit_hi)
	_gdt[1] = (gdt_entry_t){0, 0, 0, 0x9A, 0x20, 0};

	// 0x10: Kernel Data Segment
	// Access: Present(1) Ring0(00) System(1) Executable(0) Direction(0)
	// 		Writable(1) Accessed(0) -> 0x92
	// Flags: 0x00 (Size64 flag must be 0 for data
	// 		segment)
	_gdt[2] = (gdt_entry_t){0, 0, 0, 0x92, 0x00, 0};

	// 0x18: User Data Segment
	// Access: Present(1) Ring3(11) System(1) Executable(0) Direction(0)
	//		Writable(1) Accessed(0) -> 0xF2
	_gdt[3] = (gdt_entry_t){0, 0, 0, 0xf2, 0, 0};

	// 0x20: User Code Segment
	// Access: Present(1) Ring3(11) System(1) Executable(1) REadable(1)
	//		Accessed(0) -> 0xFA
	// Flags: Granularity(1) Size64(1) -> 0x20
	_gdt[4] = (gdt_entry_t){0, 0, 0, 0xFA, 0x20, 0};

	// Set GDT Pointer
	_gdt_ptr.limit = sizeof(_gdt) - 1;
	_gdt_ptr.base = (uint64_t)&_gdt;

	// Load GDT and flush segments
	_load_gdt(&_gdt_ptr);
}
