### `VBOX-USAGE.md`
# 🦋 Running Chrysalis OS in VirtualBox (Linux Host)

This guide outlines the necessary configuration to run Chrysalis OS correctly and capture kernel debug logs. Since development is done on Linux, these instructions focus on a Linux host environment.

## 🛠 Virtual Machine Configuration

To ensure the kernel detects hardware correctly and avoids a **Kernel Panic** during APIC initialization, apply the following settings:

### 1. System Settings
* **Motherboard:**
    * Enable **I/O APIC** (Required: Prevents Page Faults when accessing `0xFEE0...` memory regions).
    * Enable **EFI** (Required if booting via UEFI).
* **Processor:**
    * Allocate at least **1 CPU** (2+ recommended for testing SMP/Multicore features).
    * Enable **PAE/NX**.
* **Acceleration:**
    * Paravirtualization Interface: Set to **KVM** (Optimized for Linux hosts).



### 2. Display
* **Graphics Controller:** **VMSVGA** or **VBoxVGA**.
* **Video Memory:** Minimum **32 MB**.

### 3. Serial Port (Kernel Debug Logs)
Chrysalis OS outputs debug information (ACPI, AHCI, GPU, etc.) to the first serial port.
* Go to **Serial Ports** -> **Port 1**.
* Check **Enable Serial Port**.
* **Port Mode:** Select **Raw File**.
* **Path/Address:** Enter a path on your host (e.g., `/home/$USER/chrysalis_debug.log`).

---

## 🔍 Troubleshooting

### Kernel Panic: Page Fault (CR2=0xFEE00020)
This error occurs if the kernel attempts to access the Local APIC while:
1.  **I/O APIC** is disabled in VirtualBox settings.
2.  The memory region `0xFEE00000` is not identity-mapped in the kernel's page tables.

### Boot Failure: "No bootable option found"
* Ensure your `.iso` or `.bin` is attached to the **Storage** controller (IDE or SATA).
* If using UEFI, verify the file structure follows `/EFI/BOOT/BOOTX64.EFI`.

---

## 📝 Monitoring Logs in Real-Time

Since the serial output is redirected to a raw file, you can watch the kernel boot process live from your Linux terminal:

```bash
# Monitor the log file as it's being written
tail -f ~/chrysalis_debug.log

```

Alternatively, if you prefer a hex view for memory-specific debugging:

```bash
watch -n 1 "hexdump -C ~/chrysalis_debug.log | tail -n 20"

```
