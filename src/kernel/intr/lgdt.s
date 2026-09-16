.global _load_gdt
.type _load_gdt, @function

_load_gdt:
	# System V ABI passes the first parameter (gdt_pointer) in %rdi
	lgdt	(%rdi)

	# Reload Data segment registers with Kernel Data Selector (0x10)
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	# Flush the Code segment register (CS) to 0x08 using a far return
	pushq	$0x08		#Push kernel Code Selector
	leaq	.flush(%rip), %rax
	pushq	%rax		#Push the return instruction gdt_pointer
	lretq					# Long/Far return (64-bit)

.flush:
	ret
