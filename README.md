# Chrysalis OS

Chrysalis OS is a custom operating system built from scratch in **C/C++**, designed as a
hands-on learning and research project in low-level systems programming.
It currently provides a terminal-based interface and is architected with
future GUI support in mind.

---

## 🦋 Why “Chrysalis”?

The project was initially considered under names like **cppOS** and **herryOS**,
but neither fully captured its purpose.

The name **Chrysalis** was chosen to reflect the core idea behind the project:
**transformation**.

Just like a chrysalis represents the transitional stage before a butterfly emerges,
Chrysalis OS begins as a minimal, terminal-based system and gradually evolves
into a more complex operating system with advanced features and, eventually, a GUI.

The name symbolizes:
- gradual growth
- continuous learning
- evolution from simplicity to complexity

---

## Project Overview

- **Name:** Chrysalis OS  
- **Language:** C/C++ with x86 Assembly for bootstrapping  
- **Architecture:** x86 (i386)  
- **Bootloader:** GRUB (Multiboot-compliant)  
- **Development Platform:** Linux (Debian-based recommended)  
- **Testing:** QEMU virtual machine  
- **Current Status:** Pre-alpha / active development  

---

## Project Goals

- Build an operating system from scratch (bare metal)
- Understand low-level concepts such as:
  - boot process
  - interrupts and IRQs
  - memory management
  - paging and virtual memory
- Start with a terminal/CLI interface
- Design the kernel with future GUI and multitasking expansion in mind
- Learn by implementing, debugging, and evolving real kernel code

---

## Development Environment

### Required Packages

#### 1. Core Development Tools (Essential)
- `build-essential`
- `gcc`, `g++`
- `nasm`
- `make`
- `binutils`
- `gdb`

#### 2. QEMU Emulator
- `qemu-system-x86`
- `qemu-utils`

#### 3. Bootloader Tools
- `grub-pc-bin`
- `xorriso`
- `mtools`

#### 4. Optional Tools
- `git`
- `clang`
- `lldb`
- `valgrind`
- `cmake` (optional, future use)

---

## Installation

A helper script (`install.sh`) is provided to simplify environment setup.

The script:
- Detects the Linux distribution (Debian, Ubuntu, Arch, Fedora, etc.)
- Allows selective installation of package groups
- Handles package name differences between distros
- Can remove installed packages if needed

### Usage

```bash
chmod +x install.sh
sudo ./install.sh
````

The script presents an interactive menu for package management.

---

## Verifying GRUB Tools

Check that GRUB utilities are installed:

```bash
which grub-mkrescue
```

Expected output:

```text
/usr/bin/grub-mkrescue
```

If missing (Debian/Ubuntu):

```bash
sudo apt install grub-pc-bin xorriso mtools
```

---

## Architecture Decisions

### Why GRUB?

GRUB (Grand Unified Bootloader) was chosen because:

* It is not Linux-specific
* Fully supports the Multiboot specification
* Handles low-level boot complexity
* Is well-tested and widely used
* Allows focusing on kernel development instead of custom boot code

**Important note:**
GRUB is **not installed on the host system**.
Its binaries are bundled directly into the bootable ISO image using `grub-mkrescue`.

---

## Project Structure (Planned / Current)

```
chrysalis/
├── boot/                # Bootloader configuration
│   └── grub.cfg
├── kernel/              # Kernel source code
│   ├── kernel.cpp       # Kernel entry point
│   ├── interrupts/      # ISR / IRQ handling
│   ├── drivers/         # Hardware drivers (PIT, keyboard, etc.)
│   └── arch/            # Architecture-specific code (x86)
├── include/             # Kernel headers
├── lib/                 # Kernel-side library code
├── build/               # Compiled object files (generated)
├── iso/                 # ISO build output (generated)
│   └── boot/grub/
└── Makefile
```

---

## Development Workflow

Typical workflow during development:

1. Write or modify kernel code
2. Run `make` to build the kernel and ISO image
3. Run `make run` to boot the OS in QEMU
4. Debug using serial output or GDB
5. Iterate and improve

---

## Technical Notes

* **Multiboot:** The kernel follows the Multiboot specification so GRUB can load it
* **Freestanding Environment:** The kernel is built without relying on a host OS
* **Interrupt Safety:** Special care is taken to avoid race conditions during early boot
* **Testing:** QEMU provides a safe sandbox for kernel experimentation

---

## Resources & Documentation

* OSDev Wiki — [https://wiki.osdev.org/](https://wiki.osdev.org/)
* Multiboot Specification — [https://www.gnu.org/software/grub/manual/multiboot/multiboot.html](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
* Intel® 64 and IA-32 Architectures Software Developer’s Manuals
* GRUB Manual — [https://www.gnu.org/software/grub/manual/](https://www.gnu.org/software/grub/manual/)

---

## License

MIT License (see `LICENSE` file).

---

## Contributing

Chrysalis OS is primarily a personal learning and research project.
Contributions, suggestions, and constructive feedback are welcome,
but expectations should align with its experimental nature.

---

**Project Status:** Pre-alpha
**Current Version:** 0.0.1
**Last Updated:** December 31, 2025

```

