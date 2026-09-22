DEBUG_FLAGS ?=
export DEBUG_FLAGS
ROOT_DIR = $(CURDIR)
export ROOT_DIR
BUILD_DIR = $(ROOT_DIR)/build
export BUILD_DIR
GNU_EFI_DIR = $(ROOT_DIR)/gnu-efi
export GNU_EFI_DIR
SRC_DIR = $(ROOT_DIR)/src
export SRC_DIR
STUB_DIR = $(SRC_DIR)/bootloader
export STUB_DIR
DRIVERS_DIR = $(SRC_DIR)/drivers
export DRIVERS_DIR
KERNEL_DIR = $(SRC_DIR)/kernel
export KERNEL_DIR
KERNEL = $(BUILD_DIR)/qios.elf
export KERNEL
LIBK_DIR = $(SRC_DIR)/libk
export LIBK_DIR
LIBK = $(BUILD_DIR)/libk.a
export LIBK
EFI_IMG = $(BUILD_DIR)/BOOTX64.EFI
export EFI_IMG
ESP_IMG = $(BUILD_DIR)/esp.img
export ESP_IMG
GNUEFI_OUT_DIR = $(BUILD_DIR)/gnu-efi
export GNUEFI_OUT_DIR
GNU_OUT_A = $(GNUEFI_OUT_DIR)/libefi.a
export GNU_OUT_A

.PHONY: all clean print tests font

all: $(GNU_OUT_A) $(EFI_IMG) $(KERNEL) $(LIBK)
	# 1. Create a clean 64MB file filled with zeroes
	# 2. Create a modern GPT partition table on the image
	# 3. Create a primary partition aligned to 2048 sectors (1MiB offset)
	# We stop at 1% to ensure it fits perfectly inside the 64MB bounds
	# 4. Set the partition type to "EFI System Partition" (ESP)
	# 5. Format the partition inside the image as FAT32
	# The @@1M syntax tells mtools to look exactly at the 1 Megabyte partition offset
	dd if=/dev/zero of=$(ESP_IMG) bs=512K count=2048
	parted -s $(ESP_IMG) mklabel gpt
	parted -s $(ESP_IMG) mkpart primary fat32 1MiB 100MiB
	parted -s $(ESP_IMG) set 1 esp on
	mformat -i $(ESP_IMG)@@1M -F -v "ESP" ::
	mmd -i $(ESP_IMG)@@1M ::/EFI
	mmd -i $(ESP_IMG)@@1M ::/EFI/BOOT
	mcopy -i $(ESP_IMG)@@1M $(EFI_IMG) ::/EFI/BOOT
	mmd -i $(ESP_IMG)@@1M ::/EFI/BOOT/DRIVERS
	mcopy -i $(ESP_IMG)@@1M $(DRIVERS_DIR)/* ::/EFI/BOOT/DRIVERS
	mmd -i $(ESP_IMG)@@1M ::/EFI/BOOT/IMAGES
	mcopy -i $(ESP_IMG)@@1M $(ROOT_DIR)/resources/images/* ::/EFI/BOOT/IMAGES
	mcopy -i $(ESP_IMG)@@1M $(KERNEL) ::/EFI/BOOT
	parted -s $(ESP_IMG) mkpart "root" ext4 100MiB 800MiB
	dd if=/dev/zero of=build/ext4.raw bs=512K count=1400
	mkfs.ext4 -F -F -L "ROOT" build/ext4.raw
	debugfs -w -R "mkdir /sys" build/ext4.raw
	debugfs -w -R "write $(KERNEL) /sys/qios.elf" build/ext4.raw
	dd if=build/ext4.raw of=$(ESP_IMG) bs=512K seek=200 conv=notrunc
	@echo "Build complete."

$(GNU_OUT_A): | $(GNUEFI_OUT_DIR)
	$(MAKE) -C $(GNU_EFI_DIR) ARCH=x86_64 CROSS_COMPILE=x86_64-w64-mingw32-

# Sources have to be prerequisites: without them make never rebuilds the
# bootloader after an edit and the ESP silently keeps the old BOOTX64.EFI.
STUB_SRCS = $(wildcard $(STUB_DIR)/*.c) $(wildcard $(STUB_DIR)/*.s) \
	$(wildcard $(SRC_DIR)/include/*.h)
LIBK_SRCS = $(shell find $(LIBK_DIR) -name '*.c' -o -name '*.s')

$(EFI_IMG): $(STUB_SRCS) | $(GNU_OUT_A)
	git submodule init
	git submodule update
	$(MAKE) -C $(STUB_DIR)

$(LIBK): $(LIBK_SRCS)
	$(MAKE) -C $(LIBK_DIR)

# Same trap as the bootloader: without the sources as prerequisites, make sees
# an up-to-date qios.elf and never invokes the kernel Makefile after an edit.
KERNEL_SRCS = $(shell find $(KERNEL_DIR) -name '*.c' -o -name '*.s') \
	$(shell find $(SRC_DIR)/include -name '*.h')

$(KERNEL): $(LIBK) $(KERNEL_SRCS)
	$(MAKE) -C $(KERNEL_DIR)

$(GNUEFI_OUT_DIR):
	mkdir -p $(GNUEFI_OUT_DIR)

print:
	@echo "BUILD_DIR: $(BUILD_DIR)"
	@echo "SRC_DIR: $(SRC_DIR)"
	@echo "STUB_DIR: $(STUB_DIR)"
	@echo "DRIVERS_DIR: $(DRIVERS_DIR)"
	@echo "STAGING_DIR: $(STAGING_DIR)"
	@echo "KERNEL: $(KERNEL)"
	@echo "EFI_IMAGE: $(EFI_IMAGE)"
	@echo "ESP_IMG: $(ESP_IMG)"
	@echo "ELF_IMG: $(ELF_IMG)"
	@echo "ELF_SIZE: $(ELF_SIZE)"
	$(MAKE) -C $(STUB_DIR) print

clean:
	$(MAKE) -C $(GNU_EFI_DIR) clean
	$(MAKE) -C $(STUB_DIR) clean
# 	$(MAKE) -C $(SRC_DIR) clean
	rm -f $(ESP_IMG) $(ELF_IMG)
	rm -fr $(BUILD_DIR)

# ========== Host unit tests ==========
HOST_CC = gcc
HOST_CFLAGS = -g -Wall -Isrc/include
HOST_LIBK = src/libk/stdio/vsprintf.c src/libk/stdio/vsnprintf.c \
            src/libk/stdlib/itoa.c src/libk/stdlib/atoi.c \
            src/libk/string/reverse.c src/libk/string/strchr.c \
            src/libk/string/strlen.c src/libk/string/strncpy.c

tests: test_vsprintf test_ringbuf test_klog test_fb
	./test_vsprintf
	./test_ringbuf
	./test_klog
	./test_fb

test_vsprintf: tests/test_vsprintf.c $(HOST_LIBK)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/test_vsprintf.c $(HOST_LIBK)

test_ringbuf: tests/test_ringbuf.c src/include/kernel/ringbuf.h
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/test_ringbuf.c

# klog.c is built against tests/mock/kernel/spinlock.h so the tests can run
# as an unprivileged process (the real lock executes cli/hlt).
test_klog: tests/test_klog.c src/kernel/log/klog.c $(HOST_LIBK) tests/mock/kernel/spinlock.h
	$(HOST_CC) -Itests/mock $(HOST_CFLAGS) -o $@ tests/test_klog.c src/kernel/log/klog.c $(HOST_LIBK)

test_fb: tests/test_fb.c src/kernel/graphics/fb.c src/kernel/graphics/fb_console.c \
         src/kernel/graphics/font8x16.c src/include/kernel/devices/fb.h
	$(HOST_CC) -Itests/mock $(HOST_CFLAGS) -o $@ tests/test_fb.c \
		src/kernel/graphics/fb.c src/kernel/graphics/fb_console.c \
		src/kernel/graphics/font8x16.c src/kernel/log/klog.c $(HOST_LIBK)

font:
	python3 tools/genfont.py resources/fonts/JetBrainsMonoNerdFont-Regular.ttf \
        src/kernel/graphics/font8x16.c src/include/kernel/devices/font8x16.h \
        --size 8x16 --coverage --preview font.png