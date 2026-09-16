#pragma once

#include <global.h>
#include <stdint.h>

extern void __attribute__((sysv_abi)) _load_gdt(gdtptr_t *gdt_ptr);

void gdt_init();