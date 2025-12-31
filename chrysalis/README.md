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
