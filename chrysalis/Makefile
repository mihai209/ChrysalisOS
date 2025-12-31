# Chrysalis OS - Makefile

AS = nasm
CC = gcc
CXX = g++
LD = ld

ASFLAGS = -f elf32
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
CXXFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-use-cxa-atexit
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Source files
BOOT_SRC = boot/boot.asm
KERNEL_SRC = kernel/kernel.cpp kernel/terminal.cpp

# Object files
BOOT_OBJ = build/boot.o
KERNEL_OBJ = build/kernel.o build/terminal.o

# Output
KERNEL_BIN = build/chrysalis.bin
ISO_FILE = chrysalis.iso

.PHONY: all clean run iso

all: iso

# Assemble boot code
build/boot.o: boot/boot.asm
	@mkdir -p build
	$(AS) $(ASFLAGS) $< -o $@

# Compile kernel
build/kernel.o: kernel/kernel.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/terminal.o: kernel/terminal.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link kernel
$(KERNEL_BIN): $(BOOT_OBJ) $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

# Create bootable ISO
iso: $(KERNEL_BIN)
	@mkdir -p iso/boot
	@cp $(KERNEL_BIN) iso/boot/chrysalis.bin
	grub-mkrescue -o $(ISO_FILE) iso
	@echo "ISO created: $(ISO_FILE)"

# Run in QEMU
run: iso
	qemu-system-i386 -cdrom $(ISO_FILE)

# Clean build files
clean:
	rm -rf build/*.o build/*.bin $(ISO_FILE)
	rm -rf iso/boot/chrysalis.bin

# Full clean (including iso directory)
distclean: clean
	rm -rf build iso/boot/chrysalis.bin
