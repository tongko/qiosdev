#include <kernel/devices/fb.h>
#include <kernel/global.h>
#include <kernel/idt.h>
#include <kernel/io.h>
#include <kernel/klog.h>
#include <kernel/serial.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <stdint.h>

// 8259 PIC.  Its lines are masked in idt_init() until a driver remaps the PIC
// to vectors 0x20..0x2F and unmasks the line it cares about.
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

static const char *const exception_names[EXCEPTION_COUNT] = {
	"divide error",
	"debug",
	"nmi",
	"breakpoint",
	"overflow",
	"bound range",
	"invalid opcode",
	"device not available",
	"double fault",
	"coprocessor",
	"invalid tss",
	"segment not present",
	"stack-segment",
	"general protection",
	"page fault",
	"reserved",
	"x87 fpu",
	"alignment check",
	"machine check",
	"simd fpu",
	"virtualization",
	"control protection",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
};

// Handlers registered by drivers, indexed by vector.  The dispatcher passes
// each one the cookie it registered with - that is how an ISR reaches its own
// device without a global.
static struct {
	irq_fn_t fn;
	void *ctx;
} irq_handlers[IDT_VECTORS];

static volatile bool panicking; // a fault inside the panic path: do not recurse
static bool irq_seen[IDT_VECTORS]; // report each unexpected vector only once

void set_idt_gate(int32_t v, uint64_t isr_addr, uint8_t attrs) {
	_idt[v].isr_low = (uint16_t)(isr_addr & 0xFFFF);
	_idt[v].kernel_cs = 0x08; // Matches your GDT Kernel Code index
	_idt[v].ist = 0;
	_idt[v].attributes = attrs;
	_idt[v].isr_mid = (uint16_t)((isr_addr >> 16) & 0xFFFF);
	_idt[v].isr_high = (uint32_t)((isr_addr >> 32) & 0xFFFFFFFF);
	_idt[v].reserved = 0;
}

void irq_register(uint8_t vector, irq_fn_t fn, void *ctx) {
	if (vector < IRQ_VECTOR_MIN || vector > IRQ_VECTOR_MAX) {
		return;
	}

	irq_handlers[vector].fn = fn;
	irq_handlers[vector].ctx = ctx;
}

void irq_unregister(uint8_t vector) {
	irq_register(vector, NULL, NULL);
}

static uint64_t read_cr2(void) {
	uint64_t cr2;

	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
	return cr2;
}

static void pic_eoi(uint8_t vector) {
	if (vector >= 40) {
		outb(PIC2_CMD, PIC_EOI); // slave first
	}
	outb(PIC1_CMD, PIC_EOI); // then master
}

// --- interrupt controllers ---
// Do not touch the LAPIC from here.  Its x2APIC MSR window (0x800..0x8FF, which
// includes the TPR at 0x808) is only accessible when x2APIC is *enabled*:
// writing it in xAPIC mode is a #GP, which is exactly how this code used to
// fault during idt_init().  The xAPIC MMIO window (0xFEE00000) is not mapped by
// us yet either.  Instead every one of the 256 vectors gets an IDT entry, so a
// stray LAPIC/IOAPIC interrupt is reported with its vector rather than turning
// into a #GP somewhere unrelated - and that vector is what tells you which
// source to mask next.

// Reports a CPU exception and halts.  Never returns.
static __attribute__((noreturn)) void report_exception(interrupt_frame_t *f) {
	char line[192];

	// Stop taking interrupts.  If a panicked machine keeps them enabled, the
	// next timer interrupt becomes a second fault and its report overwrites
	// this one on screen - which is how the original RIP gets lost.
	__asm__ volatile("cli");

	if (f->vector == 14) {
		snprintf(line,
					sizeof(line),
					"[qios] EXCEPTION #PF err=0x%llx rip=0x%llx cr2=0x%llx\n",
					(unsigned long long)f->error_code,
					(unsigned long long)f->rip,
					(unsigned long long)read_cr2());
	} else {
		snprintf(line,
					sizeof(line),
					"[qios] EXCEPTION %s (#%llu) err=0x%llx rip=0x%llx cs=0x%llx rflags=0x%llx\n",
					exception_names[f->vector],
					(unsigned long long)f->vector,
					(unsigned long long)f->error_code,
					(unsigned long long)f->rip,
					(unsigned long long)f->cs,
					(unsigned long long)f->rflags);
	}

	// 1. The UART first: it has no state worth corrupting, so the message
	//    survives even when the screen is what faulted.
	serial_write(line, strlen(line));
	// 2. Then the screen, through the console's own singleton.  Lock-free,
	//    allocation-free, and a no-op if the console never came up.
	fb_panic_screen("qios: CPU EXCEPTION", line);
	// 3. Record it in the log ring, flush the backlog, and never come back.
	log_panic("%s", line);
}

// Called from isr_common_stub() with rdi = &frame.
void c_interrupt_handler(interrupt_frame_t *f) {
	if (f->vector < EXCEPTION_COUNT) {
		if (panicking) {
			// A second fault while reporting one.  The console we are drawing
			// on is the usual suspect: mark it dead and stop, rather than
			// recursing into the same fault forever.
			static const char nested[] = "[qios] nested fault while panicking, halting\n";

			serial_write(nested, sizeof(nested) - 1);
			fb_console_mark_dead();
			for (;;) {
				halt();
			}
		}

		panicking = true;
		report_exception(f); // does not return
	}

	// Vectors 32..255 are hardware interrupts.  Every one of them has an IDT
	// entry now, so an unexpected vector is reported (once, so a storm cannot
	// fill the ring) rather than turning into a #GP somewhere else.
	irq_fn_t fn = irq_handlers[f->vector].fn;

	if (fn != NULL) {
		fn(irq_handlers[f->vector].ctx, f);
	} else if (!irq_seen[f->vector]) {
		irq_seen[f->vector] = true;
		printk("irq: unexpected vector %llu: no handler, source not masked, no EOI sent",
					 (unsigned long long)f->vector);
	}

	// Only the legacy 8259 can be EOI'd today: for anything above vector 47 the
	// EOI lives in the LAPIC, whose MSR window needs x2APIC and whose MMIO
	// window is not mapped yet.  The source therefore keeps firing until a
	// driver masks it - which is what the vector in the report above is for.
	if (f->vector <= IRQ_VECTOR_MAX) {
		pic_eoi((uint8_t)f->vector);
	}
}

void idt_init(void) {
	// Every vector gets an entry: the 32 CPU exceptions so a fault reports
	// itself instead of triple faulting into a silent reboot, and 32..255 so a
	// stray hardware interrupt is reported by c_interrupt_handler instead of
	// being delivered as a #GP at whatever instruction happened to be running.
	for (int v = 0; v < IDT_VECTORS; v++) {
		set_idt_gate(v, (uint64_t)_exception_stub_table[v], 0x8E); // 0x8E: present, ring 0, 64-bit interrupt gate
	}

	// Mask every PIC line.  Until the PIC is remapped to 0x20..0x2F an unmasked
	// IRQ would arrive on vector 8..15 and be taken for an exception.  A driver
	// remaps the PIC and unmasks its own line when it is ready for interrupts.
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);

	_idt_ptr.limit = (sizeof(idt_entry_t) * IDT_VECTORS) - 1; // the limit is size - 1
	_idt_ptr.base = (uint64_t)&_idt;

	_load_idt(&_idt_ptr);

	// Enable hardware interrupt on the CPU.  The PIC lines stay masked until a
	// driver remaps the PIC and unmasks its own line, and the APIC is left
	// exactly as the firmware left it (see the note at the top of this file).
	__asm__ volatile("sti");
}
