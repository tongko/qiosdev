# === Directory ===
OUTPUT_DIR = $(BUILD_DIR)/bootloader

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, $(OUTPUT_DIR)/%.o, $(SRCS))

CFLAGS = -I$(SRC_DIR)/include -I$(GNU_EFI_DIR)/inc \
	-I$(GNU_EFI_DIR)/inc/x86_64 \
	-I$(GNU_EFI_DIR)/inc/protocol \
	-ffreestanding -fno-stack-protector -fno-stack-check \
	-fshort-wchar -mno-red-zone -maccumulate-outgoing-args \
	-DGNU_EFI_USE_MS_ABI -std=c23 -Wall -Wextra \
	-Werror -Wno-error=pragmas -O2 -g $(DFLAGS)

LDFLAGS1 = -nostdlib \
	--entry=efi_main --subsystem=10 -Bsymbolic \
  --warn-common --no-undefined --fatal-warnings \
  -L$(GNU_EFI_DIR)/x86_64/lib -L$(GNU_EFI_DIR)/x86_64/gnuefi
# 	--warn-common --no-undefined \
# 	--fatal-warnings --build-id=sha1 \
# 	-L$(BUILD_DIR)/inc/x86_64/lib \
# 	-L$(GNU_EFI_ROOT)/x86_64/gnuefi \
# 	$(GNU_EFI_ROOT)/x86_64/gnuefi/crt0-efi-x86_64.o \
# 	-Bsymbolic -entry:efi_main --subsystem,10

CC = x86_64-w64-mingw32-gcc
LD = x86_64-w64-mingw32-ld

.PHONY: all clean print

all: $(EFI_IMG)
	@echo "==========================="
	@echo "BOOT Loader build complete."
	@echo "==========================="

$(EFI_IMG): $(OBJS) | $(OUTPUT_DIR)
	@echo "LD    $< -o $@"
	$(LD) $(LDFLAGS1) $^ \
		-lefi -o $@

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	@echo "CC    $< -o $@"
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

print:
	@echo "Boot loader build configuration:"
	@echo "BUILD_DIR: $(BUILD_DIR)"
	@echo "SRCS: $(SRCS)"
	@echo "OBJS: $(OBJS)"
	@echo "STUB_SO: $(STUB_SO)"

clean:
	rm -fr $(OUTPUT_DIR)