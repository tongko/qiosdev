#include <kernel/global.h>
#include <kernel/idt.h>
#include <kernel/klog.h>
#include <kernel/serial.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <stdint.h>

static const char *const exception_names[EXCEPTION_COUNT] = {
	"divide error",	"debug",			 "nmi",						 "breakpoint",
	"overflow",			 "bound range", "invalid opcode",	 "device not available",
	"double fault",	 "coprocessor", "invalid tss",		 "segment not present",
	"stack-segment", "general protection", "page fault", "reserved",
	"x87 fpu",			 "alignment check", "machine check", "simd fpu",
	"virtualization", "control protection", "reserved", "reserved",
	"reserved",			 "reserved",		 "reserved",			 "reserved",
	"reserved",			 "reserved",		 "reserved",			 "reserved",
};

void set_idt_gate(int32_t v, uint64_t isr_addr, uint8_t attrs) {
	_idt[v].isr_low = (uint16_t)(isr_addr & 0xFFFF);
	_idt[v].kernel_cs = 0x08; // Matches your GDT Kernel Code index
	_idt[v].ist = 0;
	_idt[v].attributes = attrs;
	_idt[v].isr_mid = (uint16_t)((isr_addr >> 16) & 0xFFFF);
	_idt[v].isr_high = (uint32_t)((isr_addr >> 32) & 0xFFFFFFFF);
	_idt[v].reserved = 0;
}

static uint64_t read_cr2(void) {
	uint64_t cr2;

	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
	return cr2;
}

// Called from isr_common_stub() with rdi = &frame.
void c_interrupt_handler(interrupt_frame_t *f) {
	char line[192];

	if (f->vector >= EXCEPTION_COUNT) {
		// A PIC interrupt with no driver installed yet: note it and carry on
		// instead of taking the machine down.
		printk("irq: unhandled vector %llu", (unsigned long long)f->vector);
		return;
	}

	if (f->vector == 14) {
		snprintf(line, sizeof(line), "[qios] EXCEPTION #PF err=0x%llx rip=0x%llx cr2=0x%llx\n",
						 (unsigned long long)f->error_code, (unsigned long long)f->rip, (unsigned long long)read_cr2());
	} else {
		snprintf(line, sizeof(line), "[qios] EXCEPTION %s (#%llu) err=0x%llx rip=0x%llx cs=0x%llx rflags=0x%llx\n",
						 exception_names[f->vector], (unsigned long long)f->vector, (unsigned long long)f->error_code,
						 (unsigned long long)f->rip, (unsigned long long)f->cs, (unsigned long long)f->rflags);
	}

	// Straight to the UART first: if the fault came from the framebuffer
	// console, flushing the log through it would fault all over again.
	serial_write(line, strlen(line));

	log_panic("%s", line);
}

void idt_init(void) {
	// Install all 32 CPU exceptions, so that a fault reports itself instead of
	// triple faulting into a silent reboot.
	for (int v = 0; v < EXCEPTION_COUNT; v++) {
		set_idt_gate(v, (uint64_t)_exception_stub_table[v], 0x8E); // 0x8E: present, ring 0, 64-bit interrupt gate
	}

	// PIC IRQs 0 (timer) and 1 (keyboard).  No drivers yet; the handler logs.
	set_idt_gate(32, (uint64_t)_exception_handler_32, 0x8E);
	set_idt_gate(33, (uint64_t)_exception_handler_33, 0x8E);

	_idt_ptr.limit = (sizeof(idt_entry_t) * IDT_VECTORS) - 1; // the limit is size - 1
	_idt_ptr.base = (uint64_t)&_idt;

	_load_idt(&_idt_ptr);

	// Enable hardware interrupt on the CPU
	__asm__ volatile("sti");
}
