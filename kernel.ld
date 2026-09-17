OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)
SECTIONS
{
    . = 0xFFFFFFFF80000000;
    _kernel_virtual_start = .;

    .text : ALIGN(4096) {
        _code = .;
        *(.text)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4096);
    }

    .data : ALIGN(4096) {
        _data = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4096);
    }

    .bss : ALIGN(4096) {
        _bss = .;
        *(COMMON)
        *(.bss)
        . = ALIGN(4096);
    }

    _kernel_end = .;
}
