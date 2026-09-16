#include <kernel/idt.h>
#include <stdint.h>

idt_entry_t _idt[256];
idtptr_t _idt_ptr;

void set_idt_gate(int32_t v, uint64_t isr_addr, uint8_t attrs) {
	_idt[v].isr_low = (uint16_t)(isr_addr & 0xFFFF);
	_idt[v].kernel_cs = 0x08; // Matches your GDT Kernel Code index
	_idt[v].ist = 0;
	_idt[v].attributes = attrs;
	_idt[v].isr_mid = (uint16_t)((isr_addr >> 16) & 0xFFFF);
	_idt[v].isr_high = (uint32_t)((isr_addr >> 32) & 0xFFFFFFFF);
	_idt[v].reserved = 0;
}

// The high-levle C entry point for common stub
void c_interrupt_handler(__attribute__((unused)) uint64_t *stack_frame) {
	// TODO: print out register data or panic sceen
	// E.g. if hit divide-by-zero, draw "CPU PANIC"
	while (1) {
		__asm__ volatile("hlt");
	}
}

void idt_init() {
	// 0x8E: Present, Ring 0, 64-bit Interrupt Gate
	set_idt_gate(0, (uint64_t)_exception_handler_0, 0x8E);

	_idt_ptr.limit = (sizeof(idt_entry_t) * 256);
	_idt_ptr.base = (uint64_t)&_idt;

	_load_idt(&_idt_ptr);

	// Enable hardware interrupt on the CPU
	__asm__ volatile("sti");
}