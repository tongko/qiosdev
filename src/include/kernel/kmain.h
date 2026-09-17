#pragma once

#include <kernel/global.h>
#include <stdint.h>

void paint_background(bootinfo_t *bi, uint32_t color);
void kmain(bootinfo_t *bi);
