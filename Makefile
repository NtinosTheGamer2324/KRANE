# ==============================================================
# KRANE Makefile (Updated for Recursive src/ directory support)
# ==============================================================

ASM          := nasm
CC           := x86_64-elf-gcc
LD           := x86_64-elf-ld

BUILD_DIR    := build
SRC_DIR      := src
INC_DIR      := include

# GCC 32-bit compilation flags
CFLAGS       := -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -O2 -I$(INC_DIR)

# Linker flags
LDFLAGS      := -m elf_i386 -Ttext 0x8200 --oformat binary

STAGE1_BIN       := $(BUILD_DIR)/stage1.bin
STAGE2_BIN       := $(BUILD_DIR)/stage2.bin
STAGE2_ENTRY_BIN := $(BUILD_DIR)/stage2_entry.bin
STAGE2_C_BIN     := $(BUILD_DIR)/stage2_c.bin
IMAGE            := $(BUILD_DIR)/krane.img

IMAGE_SECTORS    := 64

# --- DYNAMIC C OBJECT FILE DISCOVERY ---
# Finds all .c files in src/ and maps them to .o files in build/
C_SOURCES := $(shell find $(SRC_DIR) -name '*.c')
C_OBJS    := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))

.PHONY: all clean

all: $(IMAGE)

# Ensure subdirectories exist in build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# --------------------------------------------------------------
# Stage 1: MBR flat binary (512 bytes)
# --------------------------------------------------------------
$(STAGE1_BIN): $(SRC_DIR)/boot/stage1.asm | $(BUILD_DIR)
	$(ASM) -f bin -o $@ $<
	@size=$$(stat -c%s $@); \
	if [ "$$size" -ne 512 ]; then \
		echo "ERROR: stage1.bin is $$size bytes, must be 512"; exit 1; \
	fi

# --------------------------------------------------------------
# Stage 2 entry trampoline (1024 bytes)
# --------------------------------------------------------------
$(STAGE2_ENTRY_BIN): $(SRC_DIR)/boot/stage2_entry.asm | $(BUILD_DIR)
	$(ASM) -f bin -o $@ $<
	@size=$$(stat -c%s $@); \
	if [ "$$size" -ne 1024 ]; then \
		echo "ERROR: stage2_entry.bin is $$size bytes, must be 1024"; exit 1; \
	fi

# --------------------------------------------------------------
# Stage 3 Startup Assembly Object (ELF 32-bit)
# --------------------------------------------------------------
$(BUILD_DIR)/stage3_start.o: $(SRC_DIR)/boot/stage3_start.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(ASM) -f elf32 -o $@ $<

# --------------------------------------------------------------
# Link Stage 3 into flat C binary blob
# --------------------------------------------------------------
$(STAGE2_C_BIN): $(BUILD_DIR)/stage3_start.o $(C_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# --------------------------------------------------------------
# Stage 2 Final (Trampoline + C blob, padded to 8192 bytes)
# --------------------------------------------------------------
$(STAGE2_BIN): $(STAGE2_ENTRY_BIN) $(STAGE2_C_BIN)
	cat $(STAGE2_ENTRY_BIN) $(STAGE2_C_BIN) > $@
	truncate -s 8192 $@
	@size=$$(stat -c%s $@); \
	if [ "$$size" -ne 8192 ]; then \
		echo "ERROR: stage2.bin is $$size bytes, must be 8192"; exit 1; \
	fi

# --------------------------------------------------------------
# Flat disk image (Stage 1 + Stage 2, padded to IMAGE_SECTORS)
# --------------------------------------------------------------
$(IMAGE): $(STAGE1_BIN) $(STAGE2_BIN)
	cat $(STAGE1_BIN) $(STAGE2_BIN) > $@
	truncate -s $$(( $(IMAGE_SECTORS) * 512 )) $@

clean:
	rm -rf $(BUILD_DIR)