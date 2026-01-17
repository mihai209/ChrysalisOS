#!/usr/bin/env python3
"""
Chrysalis OS Installer - Real Implementation
Follows Debian/Linux installation pattern
Stage: Prepare installation media with kernel + rootfs
"""

import os
import struct
import tarfile
import io

INSTALLER_DIR = "/home/mihai/Desktop/ChrysalisOS/installer"
OS_DIR = "/home/mihai/Desktop/ChrysalisOS/os"
ISO_DIR = f"{INSTALLER_DIR}/iso/install"

print("=" * 60)
print("CHRYSALIS OS INSTALLER - MEDIA PREPARATION")
print("=" * 60)

# Stage 1: Copy kernel to installation media
print("\n[STAGE 1] Copying kernel to installation media...")

kernel_src = f"{OS_DIR}/iso/boot/kernel.bin"  # Use compiled kernel from ISO
kernel_dest = f"{ISO_DIR}/bin/kernel.bin"

if os.path.exists(kernel_src):
    os.makedirs(f"{ISO_DIR}/bin", exist_ok=True)
    with open(kernel_src, 'rb') as src:
        with open(kernel_dest, 'wb') as dst:
            dst.write(src.read())
    size_kb = os.path.getsize(kernel_dest) / 1024
    print(f"OK: kernel.bin copied ({size_kb:.1f}KB)")
else:
    print(f"ERROR: {kernel_src} not found - run 'npm run build' first")
    import sys
    sys.exit(1)

# Stage 2: Create minimal rootfs structure
print("\n[STAGE 2] Creating rootfs structure...")

os.makedirs(f"{ISO_DIR}/rootfs/boot/grub", exist_ok=True)
os.makedirs(f"{ISO_DIR}/rootfs/bin", exist_ok=True)
os.makedirs(f"{ISO_DIR}/rootfs/lib", exist_ok=True)
os.makedirs(f"{ISO_DIR}/rootfs/etc", exist_ok=True)
os.makedirs(f"{ISO_DIR}/rootfs/sys", exist_ok=True)
os.makedirs(f"{ISO_DIR}/rootfs/proc", exist_ok=True)
os.makedirs(f"{ISO_DIR}/rootfs/root", exist_ok=True)

print("OK: Root directories created")

# Stage 3: Create GRUB configuration
print("\n[STAGE 3] Creating GRUB configuration...")

grub_cfg = f"""
set default=0
set timeout=5

menuentry "Chrysalis OS" {{
    echo "Loading Chrysalis OS..."
    multiboot /boot/chrysalis/kernel.bin
    boot
}}

menuentry "Chrysalis OS (Safe Mode)" {{
    echo "Loading Chrysalis OS (Safe Mode)..."
    multiboot /boot/chrysalis/kernel.bin debug
    boot
}}
"""

with open(f"{ISO_DIR}/rootfs/boot/grub/grub.cfg", 'w') as f:
    f.write(grub_cfg)

print("OK: grub.cfg written")

# Stage 4: Create root filesystem archive
print("\n[STAGE 4] Creating rootfs archive...")

rootfs_tar = f"{ISO_DIR}/rootfs.tar"
with tarfile.open(rootfs_tar, 'w') as tar:
    tar.add(f"{ISO_DIR}/rootfs", arcname='/')

tar_size_kb = os.path.getsize(rootfs_tar) / 1024
print(f"OK: rootfs.tar ({tar_size_kb:.1f}KB)")

# Stage 5: Create installation manifest
print("\n[STAGE 5] Creating installation manifest...")

manifest = f"""
# Chrysalis OS Installation Manifest
# Generated for multiboot2 installer

INSTALLER_VERSION=1.0
TARGET_ARCH=i386
TARGET_BOOTLOADER=GRUB2

FILES:
  kernel.bin={os.path.getsize(kernel_dest) if os.path.exists(kernel_dest) else 0}
  rootfs.tar={os.path.getsize(rootfs_tar)}

PARTITIONS:
  primary=FAT32
  size=1GB

BOOTLOADER_TYPE=grub2_multiboot2
INSTALLATION_TYPE=real_disk

"""

with open(f"{ISO_DIR}/manifest.txt", 'w') as f:
    f.write(manifest)

print("OK: manifest.txt created")

print("\n" + "=" * 60)
print("INSTALLATION MEDIA READY")
print("=" * 60)
print(f"\nMedia contents:")
print(f"  Installation ISO: {INSTALLER_DIR}/installer.iso")
print(f"  Kernel binary:    {kernel_dest}")
print(f"  Rootfs archive:   {rootfs_tar}")
print(f"  GRUB config:      {ISO_DIR}/rootfs/boot/grub/grub.cfg")

print(f"\nNext steps:")
print(f"  1. Run: npm run build   (compile main kernel)")
print(f"  2. Run: npm run installer (boot installer)")
print(f"  3. Installer will prepare hdd.img")
print(f"  4. Run: npm run boot    (boot from hdd)")

