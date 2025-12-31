#!/bin/bash

# Chrysalis OS - Project Structure Setup Script
# Creates the complete directory structure and GRUB configuration

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}Creating Chrysalis OS project structure...${NC}"

# Create main project directory
PROJECT_NAME="chrysalis"

if [ -d "$PROJECT_NAME" ]; then
    echo -e "${YELLOW}Warning: $PROJECT_NAME directory already exists!${NC}"
    read -p "Do you want to remove it and start fresh? (y/N): " confirm
    if [[ $confirm == [yY] ]]; then
        rm -rf "$PROJECT_NAME"
    else
        echo "Exiting..."
        exit 1
    fi
fi

mkdir -p "$PROJECT_NAME"
cd "$PROJECT_NAME"

# Create directory structure
echo -e "${GREEN}Creating directories...${NC}"
mkdir -p boot
mkdir -p kernel
mkdir -p include
mkdir -p lib
mkdir -p build
mkdir -p iso/boot/grub

# Create GRUB configuration file
echo -e "${GREEN}Creating GRUB configuration...${NC}"
cat > iso/boot/grub/grub.cfg << 'EOF'
# GRUB Configuration for Chrysalis OS

set timeout=3
set default=0

menuentry "Chrysalis OS" {
    multiboot /boot/chrysalis.bin
    boot
}

menuentry "Chrysalis OS (Debug Mode)" {
    multiboot /boot/chrysalis.bin --debug
    boot
}
EOF

# Create a simple boot assembly file (multiboot header)
echo -e "${GREEN}Creating boot files...${NC}"
cat > boot/boot.asm << 'EOF'
; Chrysalis OS - Multiboot Boot Header
; This file provides the Multiboot header for GRUB

MAGIC equ 0x1BADB002
FLAGS equ 0x0
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384  ; 16 KB stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    ; Set up stack
    mov esp, stack_top
    
    ; Call C++ kernel main
    call kernel_main
    
    ; Hang if kernel returns
    cli
.hang:
    hlt
    jmp .hang
EOF

# Create kernel main file
echo -e "${GREEN}Creating kernel files...${NC}"
cat > kernel/kernel.cpp << 'EOF'
// Chrysalis OS - Kernel Entry Point
// This is where the kernel starts executing

#include "../include/terminal.h"

extern "C" void kernel_main() {
    // Initialize terminal
    terminal_initialize();
    
    // Clear screen and print welcome message
    terminal_clear();
    terminal_writestring("Welcome to Chrysalis OS!\n");
    terminal_writestring("Version: 0.1.0 (Terminal Mode)\n");
    terminal_writestring("\n");
    terminal_writestring("System initialized successfully.\n");
    terminal_writestring("Kernel loaded and running...\n");
    
    // Halt - we'll add more functionality later
    while(1) {
        asm volatile("hlt");
    }
}
EOF

# Create terminal header
echo -e "${GREEN}Creating header files...${NC}"
cat > include/terminal.h << 'EOF'
// Chrysalis OS - Terminal Header
// VGA text mode terminal functions

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stddef.h>

// VGA text mode colors
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

void terminal_initialize();
void terminal_clear();
void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_writestring(const char* data);

#endif
EOF

# Create terminal implementation
cat > kernel/terminal.cpp << 'EOF'
// Chrysalis OS - Terminal Implementation
// VGA text mode terminal driver

#include "../include/terminal.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

void terminal_initialize() {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*) 0xB8000;
}

void terminal_clear() {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
}

void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = 0;
        }
        return;
    }
    
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry(c, terminal_color);
    
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = 0;
        }
    }
}

void terminal_writestring(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++) {
        terminal_putchar(data[i]);
    }
}
EOF

# Create linker script
echo -e "${GREEN}Creating linker script...${NC}"
cat > linker.ld << 'EOF'
/* Chrysalis OS - Linker Script */

ENTRY(_start)

SECTIONS
{
    . = 1M;
    
    .text ALIGN(4K) : {
        *(.multiboot)
        *(.text)
    }
    
    .rodata ALIGN(4K) : {
        *(.rodata)
    }
    
    .data ALIGN(4K) : {
        *(.data)
    }
    
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }
}
EOF

# Create Makefile
echo -e "${GREEN}Creating Makefile...${NC}"
cat > Makefile << 'EOF'
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
EOF

# Create README
echo -e "${GREEN}Creating README...${NC}"
cat > README.md << 'EOF'
# Chrysalis OS

A simple operating system built from scratch using C++ and Assembly.

## Project Structure

```
chrysalis/
├── boot/           # Bootloader and multiboot header
├── kernel/         # Kernel source code
├── include/        # Header files
├── lib/            # Standard library implementations
├── build/          # Compiled object files
├── iso/            # ISO image creation
│   └── boot/grub/  # GRUB configuration
├── linker.ld       # Linker script
├── Makefile        # Build automation
└── README.md       # This file
```

## Building

```bash
# Build the OS
make

# Build and run in QEMU
make run

# Clean build files
make clean
```

## Requirements

- GCC cross-compiler (i686-elf)
- NASM assembler
- GRUB tools (grub-mkrescue)
- QEMU (for testing)
- xorriso, mtools

## Current Features

- Multiboot-compliant bootloader
- Basic VGA text mode terminal
- Simple kernel entry point

## Development Status

Current Version: 0.1.0 (Terminal Mode)

This is the initial release with basic boot capability and terminal output.
EOF

echo ""
echo -e "${GREEN}✓ Project structure created successfully!${NC}"
echo ""
echo -e "${BLUE}Project created in: $(pwd)${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "  cd $PROJECT_NAME"
echo "  make          # Build the OS"
echo "  make run      # Build and run in QEMU"
echo ""
echo -e "${GREEN}Happy OS development!${NC}"