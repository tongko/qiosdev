# Define the macro for exceptions that DO NOT push an error code
.macro	ISR_NOERRCODE num
.global	_exception_handler_\num
.type	_exception_handler_\num, @function
_exception_handler_\num:
	pushq	$0	# Push a dummy error code to keep stack uniform
	pushq	$\num	# Push vector number
	jmp isr_common_stub
.endm

# Define macro for exceptions that DO push error code
.macro ISR_ERRCODE num
.global	_exception_handler_\num
.type	_exception_handler_\num, @function
_exception_handler_\num:
	pushq	$\num	# Push the vector number (error code is already pushed)
	jmp isr_common_stub
.endm

# 32 Intel architecture exceptions
ISR_NOERRCODE	0	# Divide-by-zero
ISR_NOERRCODE	1	# Debug
ISR_NOERRCODE	2	# Non-maskable Interrupt
ISR_NOERRCODE	3	# Breakpoint
ISR_NOERRCODE	4	# Overflow
ISR_NOERRCODE	5	# Bound Range Exceeded
ISR_NOERRCODE	6	# Invalid Opcode
ISR_NOERRCODE	7	# Device Not Available
ISR_NOERRCODE	8	# Double Fault
ISR_NOERRCODE	9	# Coprocessor SEgment Overrun
ISR_NOERRCODE	10	# Invalid TSS
ISR_NOERRCODE	11	# Setment Not Present
ISR_NOERRCODE	12	# Stack-Segment Fault
ISR_NOERRCODE	13	# General Protection Fault (#GP)
ISR_NOERRCODE	14	# Page Fault (#PF)
ISR_NOERRCODE	15	# Reserved
ISR_NOERRCODE	16	# x87 Floating-Point Exception
ISR_NOERRCODE	17	# Alignment Check
ISR_NOERRCODE	18	# Machine Check
ISR_NOERRCODE	19	# SIMD Floating-Point Exception
ISR_NOERRCODE	20	# Virtualization Exception
ISR_NOERRCODE	21	# Control Protection Exception
# 22 - 31 are Intel reserved exceptions
ISR_NOERRCODE	22	# Divide-by-zero
ISR_NOERRCODE	23	# Divide-by-zero
ISR_NOERRCODE	24	# Divide-by-zero
ISR_NOERRCODE	25	# Divide-by-zero
ISR_NOERRCODE	26	# Divide-by-zero
ISR_NOERRCODE	27	# Divide-by-zero
ISR_NOERRCODE	28	# Divide-by-zero
ISR_NOERRCODE	29	# Divide-by-zero
ISR_NOERRCODE	30	# Divide-by-zero
ISR_NOERRCODE	31	# Divide-by-zero

#IRQs (PIC Handlers) - Verctors 32 to 47
ISR_NOERRCODE	32	# IRQ0: Timer
ISR_NOERRCODE	33	# IRQ1: Keyboard
# TODO: Expand up to 47 for all PIC IRQs

.extern c_interrupt_handler

isr_common_stub:
	# Save CPU state (Registers)
	pushq	%r15;	pushq	%r14;	pushq	%r13;	pushq	%r12
	pushq	%r11;	pushq	%r10;	pushq	%r9;	pushq	%r8
	pushq	%rbp;	pushq	%rdi;	pushq	%rsi;	pushq	%rdx
	pushq	%rcx;	pushq	%rbx;	pushq	%rax

	# Pass the stack pointer to our C handler (System V ABI uses %rdi)
	movq	%rsp, %rdi
	call c_interrupt_handler

	# Restore CPU state
	popq	%rax;	popq	%rbx;	popq	%rcx;	popq	%rdx
	popq	%rsi;	popq	%rdi;	popq	%rbp;	popq	%r8
	popq	%r9;	popq	%r10;	popq	%r11;	popq	%r12
	popq	%r13;	popq	%r14;	popq	%r15

	addq	$16, %rsp	# Clean up the error code and vector number
	iretq
