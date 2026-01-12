# Chrysalis OS — Development Plan (Reorganized)

**Chrysalis OS** is a monolithic, educational, desktop-capable operating system designed to evolve from bare metal to a usable, networked system with a native GUI and ecosystem.

Legend:
✅ implemented 🔲 planned ☑️ partial [N] optional / nice-to-have

---

## 🟢 PHASE 1 — Core Kernel Foundation

> **Goal:** The kernel boots, survives, and talks to hardware.

### CPU & Interrupts

| Status | Component        |
| ------ | ---------------- |
| ✅      | GDT              |
| ✅      | IDT              |
| ✅      | ISR / IRQ        |
| ✅      | PIC 8259         |
| ✅      | APIC / IOAPIC    |
| ✅      | SMP (multi-core) |

### Timing & Low-level Devices

| Status | Component  |
| ------ | ---------- |
| ✅      | PIT        |
| ✅      | HPET       |
| ✅      | CMOS / RTC |
| ✅      | PC Speaker |

### Debug & Output

| Status | Component        |
| ------ | ---------------- |
| ✅      | Serial COM1      |
| ✅      | VGA Text Console |

**Result:**
The kernel is *alive*, debuggable, and stable.

---

## 🟡 PHASE 2 — Input, Events & Timing Model

> **Goal:** Structured, event-driven input usable by both shell and GUI.

| Status | Component                       |
| ------ | ------------------------------- |
| ✅      | Keyboard (IRQ1)                 |
| ✅      | Keyboard Buffer                 |
| ✅      | Keymaps (US / RO)               |
| ✅      | PS/2 Mouse (IRQ12)              |
| ✅      | Event Queue                     |
| ✅      | Timer Abstraction (`sleep(ms)`) |
| 🔲     | Calibrated Delay Loop           |

**Result:**
Hardware input → event system → consumers (shell / GUI).

---

## 🔵 PHASE 3 — Memory & Address Space

> **Goal:** Safety, isolation, and scalability.

| Status | Component               |
| ------ | ----------------------- |
| ✅      | Physical Memory Manager |
| ✅      | Paging (x86)            |
| ✅      | Virtual Memory          |
| ✅      | Kernel Heap (`kmalloc`) |
| ✅      | Slab / Buddy Allocator  |
| ✅      | User Memory Isolation   |

**Result:**
Crashes are contained, multitasking is possible.

---

## 🟣 PHASE 4 — Processes & Execution

> **Goal:** True operating system behavior.

| Status | Component                |
| ------ | ------------------------ |
| ✅      | Task / Process Structure |
| ✅      | Context Switching        |
| ✅      | Round-Robin Scheduler    |
| ✅      | Kernel Threads           |
| ✅      | User Mode (Ring 3)       |
| ✅      | Syscalls (`int 0x80`)    |
| ✅      | ELF Loader               |
| ✅      | `exec()`                 |

**Result:**
Multiple programs run independently.

---

## 🟠 PHASE 5 — Storage & Filesystems

> **Goal:** Persistence and data organization.

| Status | Component           |
| ------ | ------------------- |
| ✅      | ATA PIO             |
| ✅      | AHCI                |
| ✅      | Disk Detection      |
| ✅      | Sector Read / Write |
| ✅      | MBR Partitioning    |
| ✅      | FAT12 / FAT16       |
| ✅      | FAT32               |
| ☑️     | VFS                 |
| [N]    | Block Cache         |

**Result:**
Programs and data survive reboots.

---

## 🔴 PHASE 6 — Hardware Enablement

> **Goal:** Run on real machines, not just QEMU.

| Status | Component        |
| ------ | ---------------- |
| ✅      | PCI              |
| ✅      | ACPI             |
| ✅      | USB (UHCI)       |
| ✅      | VESA Framebuffer |
| ✅      | Basic GPU Driver |

**Result:**
Modern hardware compatibility.

---

## 🟤 PHASE 7 — Shell, CLI & Tools

> **Goal:** Productive text-based usage.

| Status | Component                  |
| ------ | -------------------------- |
| ✅      | Advanced Shell             |
| ✅      | Piping                     |
| ✅      | Scrollback                 |
| ✅      | Scripting (`.csr`, `.chs`) |
| ✅      | Virtual Terminals          |
| ✅      | Text Editor                |
| ✅      | Filesystem Tools           |
| 🔲     | Command history (↑ ↓)         |
| 🔲     | Tab completion                |
| 🔲     | Job control (`&`, `fg`, `bg`) |
| 🔲     | Environment variables         |
| 🔲     | Aliases                       |

---

## 🔶 PHASE 8 — Graphics & Desktop

> **Goal:** A usable graphical environment.

| Status | Component               |
| ------ | ----------------------- |
| ✅      | Framebuffer Abstraction |
| ✅      | Compositor              |
| ✅      | Window Manager          |
| ✅      | GUI Toolkit             |
| ✅      | Mouse-driven UI         |
| ✅      | Desktop Environment     |
| 🔲     | Window move / resize             |
| 🔲     | Window close / minimize          |
| 🔲     | Clipboard                        |
| 🔲     | Basic fonts (bitmap → TTF later) |
| 🔲     | Desktop icons                    |
| 🔲     | Simple file manager              |

---

## ⚫ PHASE 9 — Networking & Internet

> **Goal:** Real connectivity.

| Status | Component          |
| ------ | ------------------ |
| ✅      | Ethernet           |
| ✅      | DHCP               |
| 🔲     | UDP sockets        |
| 🔲     | TCP stack          |
| 🔲     | DNS resolver       |
| 🔲     | Loopback interface |
| 🔲     | `ping`             |
| 🔲     | `ifconfig`         |
| 🔲     | Network status app |

---

## 🟩 PHASE 10 — Userland & Ecosystem

> **Goal:** Make the OS extensible.

| Status | Component            |
| ------ | -------------------- |
| ☑️     | libc (freestanding)  |
| ☑️     | Package Manager      |
| 🔲     | Ports System         |
| 🔲     | Documentation System |
| 🔲     | Native Build Tools   |
| 🔲     | `/proc` filesystem             |
| 🔲     | App launcher                   |
| 🔲     | App metadata (`.desktop`-like) |

---

## 🟦 PHASE 11 — System Services & IPC

> **Goal:** Clean architecture.

| Status | Component              |
| ------ | ---------------------- |
| 🔲     | Pipes                  |
| 🔲     | Signals                |
| 🔲     | Shared Memory          |
| 🔲     | Background daemons     |
| 🔲     | Init / service manager |

---

## 🟨 PHASE 12 — Security & Stability

> **Goal:** Prevent accidents, not attackers.

| Status | Component            |
| ------ | -------------------- |
| 🔲     | Users / Groups       |
| 🔲     | File permissions     |
| 🔲     | Privilege separation |
| 🔲     | Syscall validation   |
| [N]    | Sandboxing           |

---

## 🟥 PHASE 13 — Self-Hosting (Long-term)

> **Goal:** Chrysalis builds Chrysalis.

| Status | Component       |
| ------ | --------------- |
| 🔲     | Native compiler |
| 🔲     | Native linker   |
| 🔲     | Full ports tree |

#Final Note
**Chrysalis OS** is designed as a transformation: from a simple terminal kernel → into a complete, modular, and educational operating system.

Not everything must be implemented — but everything is understood.