#pragma once
#include <bootinfo.h>

// 4 * 4096 = 16KB stack space, shoudl be enough for now...
#define STACK_PAGE_SIZE 4

typedef void __attribute((sysv_abi)) (*kernel_entry_fn_t)(bootinfo_t *);