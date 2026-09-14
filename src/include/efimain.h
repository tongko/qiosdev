#pragma once
#include <bootinfo.h>

typedef void __attribute((sysv_abi)) (*kernel_entry_fn_t)(bootinfo_t *);