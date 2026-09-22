#pragma once
#include <kernel/global.h>
#include <stdint.h>

#define IDT_VECTORS 256
#define EXCEPTION_COUNT 32

// PIC IRQs live here once the PIC has been remapped away from vectors 8..15.
#define IRQ_VECTOR_MIN 32
#define IRQ_VECTOR_MAX 47

/*
 * Frame built by isr_common_stub() (see isr.s).  The CPU pushes rip/cs/rflags
 * (plus rsp/ss when the exception came from ring 3), the stub pushes the vector
 * and - for vectors that do not supply one - a dummy error code, and the common
 * stub then pushes the 15 general purpose registers.
 */
typedef struct {
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
	uint64_t vector;		 // pushed by the stub
	uint64_t error_code; // pushed by the CPU, or a dummy 0
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
} interrupt_frame_t;

/*
 * A driver's interrupt handler.  It gets back the ctx it registered with, which
 * is how an ISR reaches its own device without a global: the dispatcher hands
 * the cookie back.  Keep the body short - queue the work, do not render.
 */
typedef void (*irq_fn_t)(void *ctx, interrupt_frame_t *frame);

// Table of all 256 entry points (exceptions and hardware vectors), built in
// isr.s.  Every vector is installed: a not-present gate turns a stray
// interrupt into a #GP whose RIP points at whatever was interrupted.
extern void (*const _exception_stub_table[IDT_VECTORS])(void);
extern void _load_idt(idtptr_t *idt_ptr);

void set_idt_gate(int32_t v, uint64_t isr_addr, uint8_t attrs);

// Register/unregister a handler for a PIC IRQ vector (IRQ_VECTOR_MIN..MAX).
void irq_register(uint8_t vector, irq_fn_t fn, void *ctx);
void irq_unregister(uint8_t vector);

void c_interrupt_handler(interrupt_frame_t *frame);
void idt_init(void);
