#include <kernel/global.h>
#include <kernel/tsc.h>

idt_entry_t _idt[256];
idtptr_t _idt_ptr;
gdt_entry_t _gdt[5];
gdtptr_t _gdt_ptr;

bootinfo_t _bi;

uint64_t _tsc_start = 0;
uint64_t _tsc_hz = 0;
