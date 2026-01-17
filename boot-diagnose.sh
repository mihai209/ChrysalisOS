#!/bin/bash
# ChrysalisOS Boot Diagnostics Script
# Tests ISO9660 mount capability and provides QEMU boot options

set -e

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║          ChrysalisOS Boot Diagnostics                         ║"
echo "║          Testing ISO9660 support and QEMU configurations      ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

PROJECT_DIR="/home/mihai/Desktop/ChrysalisOS"
ISO="$PROJECT_DIR/hybrid/chrysalis-alpine-hybrid.iso"

# Check ISO file
echo "[1] Checking ISO file..."
if [ ! -f "$ISO" ]; then
    echo "❌ ISO not found: $ISO"
    exit 1
fi

ISO_SIZE=$(ls -lh "$ISO" | awk '{print $5}')
echo "✓ ISO found: $ISO_SIZE"
echo "✓ ISO path: $ISO"
echo ""

# Check QEMU availability
echo "[2] Checking QEMU installation..."
if command -v qemu-system-i386 &> /dev/null; then
    QEMU_VERSION=$(qemu-system-i386 --version | head -1)
    echo "✓ QEMU available: $QEMU_VERSION"
else
    echo "❌ QEMU not found (qemu-system-i386)"
    echo "   Install with: sudo apt-get install qemu-system-x86"
    exit 1
fi
echo ""

# Check kernel modules
echo "[3] Checking host system ISO9660 support..."
if grep -q iso9660 /proc/filesystems; then
    echo "✓ Host supports ISO9660"
else
    echo "⚠ Host may not support ISO9660"
fi

if grep -q udf /proc/filesystems; then
    echo "✓ Host supports UDF"
else
    echo "⚠ Host may not support UDF"
fi
echo ""

# Check GRUB config
echo "[4] Checking GRUB configuration..."
GRUB_CFG="$PROJECT_DIR/hybrid/iso_root/boot/grub/grub.cfg"
if [ -f "$GRUB_CFG" ]; then
    ENTRIES=$(grep -c "menuentry" "$GRUB_CFG" || echo 0)
    echo "✓ GRUB config found with $ENTRIES boot entries"
    
    echo ""
    echo "Boot menu entries:"
    grep "menuentry" "$GRUB_CFG" | sed 's/menuentry "/  - /' | sed 's/" {//'
else
    echo "❌ GRUB config not found: $GRUB_CFG"
fi
echo ""

# Provide boot commands
echo "[5] Boot Commands"
echo "════════════════════════════════════════════════════════════════"
echo ""

echo "RECOMMENDED (Standard CDROM boot):"
echo "$ cd $PROJECT_DIR"
echo "$ qemu-system-i386 -cdrom hybrid/chrysalis-alpine-hybrid.iso -m 512 -boot d"
echo ""

echo "ALTERNATIVE 1 (With serial console):"
echo "$ qemu-system-i386 -cdrom hybrid/chrysalis-alpine-hybrid.iso -m 512 -serial stdio -nographic"
echo ""

echo "ALTERNATIVE 2 (With full debug output):"
echo "$ qemu-system-i386 -cdrom hybrid/chrysalis-alpine-hybrid.iso -m 512 -serial stdio -nographic -enable-kvm"
echo ""

echo "ALTERNATIVE 3 (For AMD systems):"
echo "$ qemu-system-i386 -cdrom hybrid/chrysalis-alpine-hybrid.iso -m 512 -machine pc -boot d"
echo ""

echo "VIRTUALBOX (if QEMU doesn't work):"
echo "$ VBoxManage createvm --name chrysalis --ostype Linux --basefolder ."
echo "$ VBoxManage modifyvm chrysalis --memory 512"
echo "$ VBoxManage storageattach chrysalis --storagectl IDE --port 1 --device 0 --type dvddrive --medium hybrid/chrysalis-alpine-hybrid.iso"
echo "$ VBoxManage startvm chrysalis"
echo ""

echo "USB BOOT (for physical hardware):"
echo "$ sudo dd if=hybrid/chrysalis-alpine-hybrid.iso of=/dev/sdX bs=4M"
echo "$ sudo sync"
echo "(Replace sdX with your USB device)"
echo ""

# Instructions if boot fails
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "IF YOU SEE ALPINE EMERGENCY SHELL:"
echo ""
echo "This indicates ISO9660 mounting failed. Try these steps:"
echo ""
echo "1. At GRUB menu, select: 'Chrysalis OS (Debug - Full Output)'"
echo "   - This explicitly preloads ISO9660 modules"
echo "   - Watch for 'insmod iso9660' in output"
echo ""
echo "2. If Debug mode works, the ISO is functional"
echo "   - Standard boot should work (may need different QEMU params)"
echo ""
echo "3. If still stuck in emergency shell:"
echo "   - Type: cat /proc/filesystems"
echo "   - Check if 'iso9660' or 'udf' is listed"
echo ""
echo "4. Check system logs after successful boot:"
echo "   - tail /var/log/chrysalis.log"
echo "   - dmesg | grep -i error"
echo ""

echo "════════════════════════════════════════════════════════════════"
echo ""
echo "More info: See GUI_EDITION_README.md [Troubleshooting > Boot Issues]"
echo ""
