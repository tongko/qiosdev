#pragma once

/*
 * Local APIC and I/O APIC, driven entirely through MMIO.
 *
 * The APIC MSR window (0x800..0x8FF) only exists when x2APIC is enabled: in
 * xAPIC mode those writes are #GP, which is how an earlier attempt at silencing
 * the timer managed to fault during idt_init().  Every register here goes
 * through the memory mapped window the MADT points at, mapped uncached.
 */

#include <stdbool.h>
#include <stdint.h>

/*
 * Map the controllers, enable the local APIC, mask every I/O APIC redirection
 * entry, and pre-latch the ISA IRQ -> GSI overrides.  Returns false (and leaves
 * the 8259 in charge) when the MADT gave us nothing usable.
 */
bool apic_init(void);

// True once apic_init() succeeded: interrupts arrive through the I/O APIC and
// are acknowledged with apic_eoi() instead of the 8259.
bool apic_ready(void);

// Signal end of interrupt to the local APIC.
void apic_eoi(void);

/*
 * Route an ISA IRQ to a vector and unmask that line only, applying the MADT
 * interrupt source override for it (IRQ 0 is GSI 2 on most PCs) plus its
 * polarity and trigger.  IRQs without an override keep the ISA defaults,
 * edge and active high.
 */
bool apic_route_isa_irq(uint32_t irq, uint8_t vector);

// Route a raw GSI.  mps_flags is an override's flags field, 0 for the defaults.
bool apic_route_gsi(uint32_t gsi, uint8_t vector, uint16_t mps_flags);

void apic_mask_gsi(uint32_t gsi);

/*
 * Calibrate the local APIC timer against the TSC and start a periodic tick.
 * This does three things at once: it proves the LAPIC -> IDT -> dispatcher path
 * works, it gives the idle loop something to wake up on, and it is the time
 * base a scheduler wants later.
 *
 * The vector sits inside IRQ_VECTOR_MIN..MAX only because irq_register() still
 * enforces that PIC-era range; vectors are not IRQs any more.
 */
#define APIC_TIMER_VECTOR 45
bool apic_timer_start(uint32_t hz);
uint64_t apic_timer_ticks(void);

uint64_t apic_lapic_base(void);
uint32_t apic_ioapic_base(void);
uint32_t apic_ioapic_lines(void);
uint32_t apic_cpu_apic_id(void);
