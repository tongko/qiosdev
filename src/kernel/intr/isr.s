# Exception and IRQ entry stubs.  See kernel/idt.h for the frame that
# isr_common_stub() builds.
#
# Vectors 8, 10, 11, 12, 13, 14, 17 and 21 push an error code themselves, so
# their stubs must NOT push a dummy one.  Otherwise the frame is shifted by 8
# bytes - the handler reads the wrong rip - and iretq pops the CPU's error code
# as the return address, which triple faults.

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
ISR_ERRCODE	8	# Double Fault (pushes an error code)
ISR_NOERRCODE	9	# Coprocessor Segment Overrun
ISR_ERRCODE	10	# Invalid TSS (pushes an error code)
ISR_ERRCODE	11	# Segment Not Present (pushes an error code)
ISR_ERRCODE	12	# Stack-Segment Fault (pushes an error code)
ISR_ERRCODE	13	# General Protection Fault (pushes an error code)
ISR_ERRCODE	14	# Page Fault (pushes an error code)
ISR_NOERRCODE	15	# Reserved
ISR_NOERRCODE	16	# x87 Floating-Point Exception
ISR_ERRCODE	17	# Alignment Check (pushes an error code)
ISR_NOERRCODE	18	# Machine Check
ISR_NOERRCODE	19	# SIMD Floating-Point Exception
ISR_NOERRCODE	20	# Virtualization Exception
ISR_ERRCODE	21	# Control Protection Exception (pushes an error code)
ISR_NOERRCODE	22	# Reserved
ISR_NOERRCODE	23	# Reserved
ISR_NOERRCODE	24	# Reserved
ISR_NOERRCODE	25	# Reserved
ISR_NOERRCODE	26	# Reserved
ISR_NOERRCODE	27	# Reserved
ISR_NOERRCODE	28	# Reserved
ISR_NOERRCODE	29	# Reserved
ISR_NOERRCODE	30	# Reserved
ISR_NOERRCODE	31	# Reserved

# IRQs (PIC Handlers) - Vectors 32 to 47
ISR_NOERRCODE	32	# IRQ0: Timer
ISR_NOERRCODE	33	# IRQ1: Keyboard
# TODO: Expand up to 47 for all PIC IRQs

# Table of the 32 exception entry points, so idt_init() can install them all.
.section .rodata
.align 8
.global	_exception_stub_table
_exception_stub_table:
	.quad	_exception_handler_0
	.quad	_exception_handler_1
	.quad	_exception_handler_2
	.quad	_exception_handler_3
	.quad	_exception_handler_4
	.quad	_exception_handler_5
	.quad	_exception_handler_6
	.quad	_exception_handler_7
	.quad	_exception_handler_8
	.quad	_exception_handler_9
	.quad	_exception_handler_10
	.quad	_exception_handler_11
	.quad	_exception_handler_12
	.quad	_exception_handler_13
	.quad	_exception_handler_14
	.quad	_exception_handler_15
	.quad	_exception_handler_16
	.quad	_exception_handler_17
	.quad	_exception_handler_18
	.quad	_exception_handler_19
	.quad	_exception_handler_20
	.quad	_exception_handler_21
	.quad	_exception_handler_22
	.quad	_exception_handler_23
	.quad	_exception_handler_24
	.quad	_exception_handler_25
	.quad	_exception_handler_26
	.quad	_exception_handler_27
	.quad	_exception_handler_28
	.quad	_exception_handler_29
	.quad	_exception_handler_30
	.quad	_exception_handler_31

.text
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
