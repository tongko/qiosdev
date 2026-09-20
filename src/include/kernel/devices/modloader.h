#pragma once

#include <kernel/devices/device.h>
#include <kernel/devices/extfs.h>
#include <stdint.h>
#include <stddef.h>

// Load ELF driver binary from ext4 filesystem, relocate, register driver into device tree
driver_t *mod_load(ext4_fs_t *fs, const char *filee_path);
void mod_unload(driver_t *drv);