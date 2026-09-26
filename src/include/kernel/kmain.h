#pragma once

#include <kernel/acpi.h>
#include <kernel/apic.h>
#include <kernel/bmp.h>
#include <kernel/devices/device.h>
#include <kernel/devices/fb.h>
#include <kernel/devices/kbd.h>
#include <kernel/gdt.h>
#include <kernel/global.h>
#include <kernel/idt.h>
#include <kernel/io.h>
#include <kernel/klog.h>
#include <kernel/mm.h>
#include <kernel/serial.h>
#include <kernel/tsc.h>
#include <libk/string.h>
#include <stdint.h>

void paint_background(bootinfo_t *bi, uint32_t color);
void kmain(bootinfo_t *bi);
