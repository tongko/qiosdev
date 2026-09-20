#pragma once
#include <kernel/global.h>
#include <stdint.h>

#define IDT_VECTORS 256
#define EXCEPTION_COUNT 32

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

// Table of the 32 exception entry points, built in isr.s.
extern void (*const _exception_stub_table[EXCEPTION_COUNT])(void);
extern void _exception_handler_32(void); // IRQ0 timer
extern void _exception_handler_33(void); // IRQ1 keyboard
extern void _load_idt(idtptr_t *idt_ptr);

void set_idt_gate(int32_t v, uint64_t isr_addr, uint8_t attrs);
void c_interrupt_handler(interrupt_frame_t *frame);
void idt_init(void);
