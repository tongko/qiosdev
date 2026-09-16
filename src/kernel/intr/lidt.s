.global	_load_idt
.type _load_idt, @function

_load_idt:
	lidt	(%rdi)
	ret
	