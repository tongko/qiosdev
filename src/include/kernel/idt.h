#pragma once
#include <kernel/global.h>
#include <stdint.h>

extern void _exception_handler_0(void);
extern void _load_idt(idtptr_t *idt_ptr);
void set_idt_gate(int32_t v, uint64_t isr_addr, uint8_t attrs);
void c_interrupt_handler(uint64_t *stack_frame);
void idt_init();
