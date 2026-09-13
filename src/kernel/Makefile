# === Directory ===
OBJS_DIR = $(BUILD_DIR)/kernel

CC = x86_64-elf-gcc
LD = x86_64-elf-ld

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, $(OBJS_DIR)/%.o, $(SRCS))
OUTPUT = $(BUILD_DIR)/qios.elf

CFLAGS = -ffreestanding \
          -mcmodel=kernel \
          -mno-red-zone \
          -mno-mmx -mno-sse -mno-sse2 \
          -fno-stack-protector \
          -fno-pie -fno-pic \
          -Wall -Wextra -Werror \
          -std=c11 -O2 -g \
					-I$(SRC_DIR)/include \
					-I$(GNU_EFI_DIR)/inc

LDFLAGS = -nostdlib \
           -z max-page-size=0x1000 \
           -static \
           -T ../../kernel.ld

.PHONY: all clean print

all: $(OUTPUT)
	@echo "======================"
	@echo "Kernel build complete."
	@echo "======================"

$(OUTPUT): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/kernel/%.o: $(SRCS) | $(OBJS_DIR)
	$(CC) $(CFLAGS) -c -o $@ $^

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

print:
	@echo "Kernel build configuration:"
	@echo "BUILD_DIR: $(BUILD_DIR)"
	@echo "SRCS     : $(SRCS)"
	@echo "OBJS     : $(OBJS)"
	@echo "OUTPUT   : $(OUTPUT)"

clean:
	rm -rf $(OBJS_DIR)
	rm -f $(OUTPUT)