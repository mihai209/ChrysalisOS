
CHRYSALIS OS - ALPINE HYBRID BOOT IMAGE
========================================

This ISO contains:
- Alpine Linux 3.19 kernel (vmlinuz-virt 6.6)
- Busybox 1.36.1 shell and utilities
- Chrysalis OS GUI framework and code

BOOT OPTIONS:
1. Chrysalis OS (Alpine + Busybox + Chrysalis)
   - Full system with Chrysalis GUI
   - Recommended for most users

2. Chrysalis OS (verbose)
   - Same as above but with debug output
   - Use for troubleshooting

3. Alpine Linux shell
   - Pure Alpine Linux with busybox shell
   - For advanced users

SYSTEM INFORMATION:
- Architecture: x86 (32-bit)
- Kernel: Linux 6.6-virt (Alpine)
- Shell: Busybox
- Init: Chrysalis startup script (/opt/chrysalis/init.sh)
- Filesystem: RAM-based (Alpine initramfs model)

TO INSTALL:
1. Boot from this ISO
2. Run: setup-alpine (if available)
   Or manually:
   - Mount filesystem
   - Copy /opt/chrysalis/* to /opt/chrysalis
   - Configure /etc/fstab
   - Set up bootloader

CHRYSALIS OS LOCATION:
- Source: /opt/chrysalis/
- Startup: /opt/chrysalis/init.sh
- Headers: /opt/chrysalis/kernel.h
- Code: /opt/chrysalis/*.cpp, *.h, *.c

DEFAULT BOOT:
On first boot, you'll see the GRUB menu.
Select option 1 for Chrysalis OS.

System will initialize and start the Chrysalis OS interface.

For more information:
- https://github.com/ChrysalisOS
- /opt/chrysalis/README
