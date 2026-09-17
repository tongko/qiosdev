#pragma once

#include <kernel/global.h>
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 0x1000
#define MAX_ORDER 10

void mem_init(bootinfo_t *bi);
paddr_t alloc_pages(size_t num_pg);
vaddr_t kalloc(size_t size);
