# Minimal Chrysalis OS Installer Makefile
# Purpose: Build installer ISO with setup wizard

CC = gcc
CXX = g++
AS = nasm
LD = ld

CFLAGS = -fno-builtin -nostdinc -Wall -Wextra -ffreestanding -O2 -m32 -I../os/kernel/include
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = -f elf32
LDFLAGS = -nostdlib -m elf_i386 -T ../os/linker.ld

BUILD = build
ISO = iso

# Minimal object files (installer only)
OBJS = \
	$(BUILD)/multiboot2_header.o \
	$(BUILD)/boot.o \
	$(BUILD)/kernel_installer.o \
	$(BUILD)/installer.o \
	$(BUILD)/string.o

all: $(BUILD) installer.iso

$(BUILD):
	@mkdir -p $(BUILD) $(ISO)/boot/grub

# Bootloader entry point
$(BUILD)/boot.o: ../os/kernel/boot.asm
	@echo "[BUILD] boot.asm"
	$(AS) $(ASFLAGS) $< -o $@

# Multiboot2 header
$(BUILD)/multiboot2_header.o: ../os/arch/i386/multiboot2_header.c
	@echo "[BUILD] multiboot2_header.c"
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel entry point and installer implementation
$(BUILD)/kernel_installer.o: kernel.cpp
	@echo "[BUILD] kernel.cpp (kernel entry + installer)"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Real installer implementation
$(BUILD)/installer.o: installer.cpp
	@echo "[BUILD] installer.cpp (real installation)"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# String functions (minimal installer version)
$(BUILD)/string.o: string.cpp
	@echo "[BUILD] string.cpp (minimal)"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Kernel ELF binary
$(BUILD)/installer.elf: $(OBJS)
	@echo "[LINK] Creating installer.elf"
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# ISO creation
installer.iso: $(BUILD)/installer.elf
	@echo "[ISO] Creating GRUB configuration..."
	@cp $(BUILD)/installer.elf $(ISO)/boot/
	@echo 'menuentry "Chrysalis OS - System Installer" { multiboot2 /boot/installer.elf; }' > $(ISO)/boot/grub/grub.cfg
	@echo "[ISO] Generating ISO image..."
	@grub-mkrescue -o installer.iso $(ISO) 2>&1 | grep -v "xorriso:"
	@ls -lh installer.iso | awk '{print "[ISO] Created: " $$9 " (" $$5 " bytes)"}'

clean:
	@echo "[CLEAN] Removing build artifacts..."
	@rm -rf $(BUILD) $(ISO) installer.iso

.PHONY: all clean

