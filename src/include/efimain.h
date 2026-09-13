#pragma once
#include <bootinfo.h>

typedef void(EFIAPI *kernel_entry_fn_t)(bootinfo_t *);