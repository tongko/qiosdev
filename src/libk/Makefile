# === Directory ===
OBJS_DIR = $(BUILD_DIR)/libk

CC = x86_64-elf-gcc
AR = x86_64-elf-ar

SRCS = $(shell find . -type f -name "*.c")
OBJS = $(patsubst %.c, $(OBJS_DIR)/%.o, $(SRCS))

CFLAGS = -ffreestanding \
          -mcmodel=kernel \
          -mno-red-zone \
          -mno-mmx -mno-sse -mno-sse2 \
          -fno-stack-protector \
          -fno-pie -fno-pic \
          -Wall -Wextra -Werror \
          -std=c11 -O2 -g \
		  -I$(SRC_DIR)/include

OUTPUT = $(BUILD_DIR)/libk.a

.PHONY: all clean

all: $(OUTPUT)
	@echo "===================="
	@echo "LibK build complete."
	@echo "===================="

$(OUTPUT): $(OBJS)
	$(AR) rcs $@ $(OBJS)

$(OBJS_DIR)/%.o: $(LIBK_DIR)/%.c | $(OBJS_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

clean:
	rm -rf $(OBJS_DIR)
	rm -f $(OUTPUT)


