# Chrysalis OS — Development Plan

This document describes the long-term technical roadmap of **Chrysalis OS**.
Each level represents a meaningful evolutionary step, from a minimal kernel to a fully usable operating system.

Legend:
✅ = implemented
🔲 = planned
[N] = optional / nice-to-have

---

## 🟢 LEVEL 1 — Foundation (Minimal Working Kernel)

> Goal: Boot reliably, handle interrupts, and interact with basic hardware.

| Status | Component         | Notes              |
| ------ | ----------------- | ------------------ |
| ✅      | GDT               | Correct            |
| ✅      | IDT               | OK                 |
| ✅      | ISR / IRQ         | Stable             |
| ✅      | PIC 8259          | Properly remapped  |
| ✅      | PIT (Timer)       | Functional         |
| ✅      | Keyboard (IRQ1)   | Working            |
| ✅      | VGA Text Terminal | Stable             |
| ✅      | Serial COM1       | Stable (debugging) |
| ✅      | CMOS / RTC        | Simple, useful     |
| ✅      | PC Speaker        | Simple + fun       |

**Why this level matters:**
Without this layer, nothing else is possible. This is where the kernel proves it can *exist*.

---

## 🟡 LEVEL 2 — Input & Timing

> Goal: Clean input handling and predictable timing — quality-of-life for the kernel.

| Status | Component         | Why it matters         |
| ------ | ----------------- | ---------------------- |
| ✅      | PS/2 Mouse        | IRQ12                  |
| ✅      | Keyboard Buffer   | Correct input handling |
| ✅      | Keymap (US / RO)  | Extensible             |
| ✅      | Timer Abstraction | `sleep(ms)`            |
| ✅      | Uptime / Ticks    | System stability       |
| 🔲     | Calibrated Delay  | Needed for drivers     |
| ✅      | Event Queue       | GUI foundation         |

**Key idea:**
Move from “hardware reacts immediately” to **structured, buffered, event-driven input**.

---

## 🔵 LEVEL 3 — Storage (First Big Leap)

> Goal: Persistent data. The OS starts to *remember*.

| Status | Component             | Comment              |
| ------ | --------------------- | -------------------- |
| ✅      | ATA PIO               | Ideal starting point |
| ✅      | HDD Detection         | IDENTIFY command     |
| ✅      | Sector Read           | Major milestone      |
| ✅      | Sector Write          | Corruption risk      |
| [N]    | Simple Cache          | Performance          |
| ✅      | Partition Table (MBR) | Required             |
| ✅      | FAT12 / FAT16         | Easy                 |
| ✅      | FAT32                 | Harder               |
| ✅      | VFS                   | Clean architecture   |

**Why this changes everything:**
With storage + VFS, user programs and real tools become possible.

---

## 🟣 LEVEL 4 — Memory Management

> Goal: Isolation, safety, and scalability.

| Status | Component               | Notes        |
| ------ | ----------------------- | ------------ |
| ✅      | Physical Memory Manager | Bitmap       |
| ✅      | x86 Paging              | Game changer |
| ✅      | Virtual Memory          | Isolation    |
| ✅      | Kernel Heap (`kmalloc`) | Mandatory    |
| ✅      | Slab / Buddy Allocator  | Optimization |
| ✅      | User Memory Isolation   | Security     |

**Key concept:**
Memory bugs stop being fatal, and multitasking becomes realistic.

---

## 🟠 LEVEL 5 — Processes & Multitasking

> Goal: The OS becomes a *real* operating system.

| Status | Component             | Notes              |
| ------ | --------------------- | ------------------ |
| ✅      | Task Structure        | Core               |
| ✅      | Context Switch        | Hard but rewarding |
| ✅      | Round-Robin Scheduler | Simple             |
| ✅      | Kernel Threads        |                    |
| ✅      | User Mode             | Ring 3             |
| ✅      | Syscalls (`int 0x80`) |                    |
| ✅      | ELF Loader            |                    |
| ✅     | `exec()`              | done              |

**This is the turning point:**
From a kernel → **a multi-process OS**.

---

## 🔴 LEVEL 6 — Advanced Hardware

> Goal: Modern hardware support and scalability.

| Status | Component         |
| ------ | ----------------- |
| ✅      | PCI Bus           |
| ✅     | ACPI              |
| ✅     | APIC / IOAPIC     |
| ✅     | SMP (Multi-core)  |
| ✅     | HPET              |
| ✅     | USB               |
| ✅     | AHCI              |
| ✅     | VESA Framebuffer  |
| ✅     | Basic GPU Support |

**Optional but impressive.**
This level separates hobby kernels from serious systems.

---

## 🟤 LEVEL 7 — UX & Tools

> Goal: Usability, developer comfort, and productivity.

| Status | Component         |
| ------ | ----------------- |
| ✅     | Advanced Shell    |
| ✅     | Piping            |
| ✅    | Scrollback        |
| ✅     | Scripting         |
| ✅     | Virtual Terminals |
| ✅     | Cursor            |
| ✅      | Colors            |
| ✅     | Text Editor       |
| ✅     | Filesystem Tools  |

---

## 🔶 LEVEL 8 — Graphics & GUI

> Goal: Visual interface and windowed environment.

| Status | Component               |
| ------ | ----------------------- |
| ✅     | Framebuffer Abstraction |
| ✅     | Basic Compositor        |
| ✅     | Window Manager          |
| ✅     | GUI Toolkit             |
| ✅     | Mouse-driven UI         |
| ✅     | Desktop Environment     |

---

## ⚫ LEVEL 9 — Userland & Ecosystem

> Goal: Self-hosting, extensibility, and community.

| Status | Component              |
| ------ | ---------------------- |
| ☑️     | libc (minimal / freestanding)|
| ✅     | POSIX-like API         |
| ✅     | Internet Support (Ethernet + DHCP)       |
| 🔲     | Package Manager        |
| 🔲     | Ports System           |
| 🔲     | Native Build Toolchain |
| 🔲     | Documentation System   |

---

## Final Note

**Chrysalis OS** is designed as a transformation:
from a simple terminal kernel
→ into a complete, modular, and educational operating system.

Not everything must be implemented —
but everything is **understood**.
