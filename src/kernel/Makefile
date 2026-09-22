# === Directory ===
OBJS_DIR = $(BUILD_DIR)/kernel

CC = x86_64-elf-gcc
LD = x86_64-elf-ld

SRCS = $(shell find -type f -name "*.c")
ASM_SRCS = $(shell find -type f -name "*.s")
OBJS = $(patsubst %.c, $(OBJS_DIR)/%.o, $(SRCS))
ASM_OBJS = $(patsubst %.s, $(OBJS_DIR)/%.asmo, $(ASM_SRCS))
OUTPUT = $(BUILD_DIR)/qios.elf

CFLAGS = -ffreestanding \
          -mcmodel=kernel \
          -mno-red-zone \
          -mno-mmx -mno-sse -mno-sse2 \
          -fno-stack-protector \
          -fno-pie -fno-pic \
          -Wall -Wextra -Werror \
          -std=c11 -O2 -g \
					-MMD -MP \
					-I$(SRC_DIR)/include \
					-I$(GNU_EFI_DIR)/inc
ASMFLAGS = -ffreestanding -m64 -g

LDFLAGS = -nostdlib \
           -z max-page-size=0x1000 \
           -static \
           -T ../../kernel.ld

.PHONY: all clean print

all: $(OUTPUT)
	@echo "======================"
	@echo "Kernel build complete."
	@echo "======================"

$(OUTPUT): $(OBJS) $(ASM_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBK)

$(BUILD_DIR)/kernel/%.asmo: $(SRC_DIR)/kernel/%.s | $(OBJS_DIR)
	mkdir -p $(dir $@)
	$(CC) $(ASMFLAGS) -c -o $@ $<

$(BUILD_DIR)/kernel/%.o: $(SRC_DIR)/kernel/%.c | $(OBJS_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

# Header dependencies written by -MMD.  Without these, editing a header (for
# instance regenerating font8x16.h with a different cell size) leaves every
# object that includes it stale, and the console keeps the old geometry
# compiled in while the font data has a different shape.
-include $(OBJS:.o=.d)

print:
	@echo "Kernel build configuration:"
	@echo "BUILD_DIR: $(BUILD_DIR)"
	@echo "SRCS     : $(SRCS)"
	@echo "OBJS     : $(OBJS)"
	@echo "OUTPUT   : $(OUTPUT)"
	@echo "ASM_SRCS : $(ASM_SRCS)"
	@echo "ASM_OBJS : $(ASM_OBJS)"

clean:
	rm -rf $(OBJS_DIR)
	rm -f $(OUTPUT)