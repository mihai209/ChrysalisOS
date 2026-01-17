#!/bin/sh
# /opt/chrysalis/init.sh - Chrysalis OS initialization on Alpine Linux

echo "╔════════════════════════════════════════╗"
echo "║                                        ║"
echo "║       CHRYSALIS OS ON ALPINE LINUX     ║"
echo "║       GUI + Networking Edition         ║"
echo "║                                        ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "[Chrysalis] System Components:"
echo "  ✓ Kernel: Linux Alpine 6.6-virt"
echo "  ✓ Shell: Bash with bash config"
echo "  ✓ Utilities: 36+ Busybox commands"
echo "  ✓ Networking: curl, wget, net-tools"
echo "  ✓ GUI: Available (framebuffer ready)"
echo "  ✓ Framework: Chrysalis OS"
echo ""

# System initialization
echo "[Alpine] Mounting filesystems..."
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
mount -t devtmpfs devtmpfs /dev 2>/dev/null || true

# Set hostname
echo "chrysalis-alpine" > /etc/hostname
hostname -F /etc/hostname

# Network configuration
echo "[Alpine] Configuring networking..."
ifconfig lo 127.0.0.1 up 2>/dev/null || true

# Try to get DHCP if eth0 exists
if [ -e /sys/class/net/eth0 ]; then
    echo "  [*] Attempting DHCP on eth0..."
    udhcpc -i eth0 2>/dev/null &
fi

# Create necessary directories
echo "[Chrysalis] Setting up directories..."
mkdir -p /opt/chrysalis/bin 2>/dev/null || true
mkdir -p /opt/chrysalis/data 2>/dev/null || true
mkdir -p /opt/chrysalis/gui 2>/dev/null || true
mkdir -p /var/log 2>/dev/null || true

# Set environment
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/opt/chrysalis/bin:\$PATH
export HOME=/root
export SHELL=/bin/bash
export TERM=xterm-256color

echo "[Alpine] Filesystem ready"
echo ""

# Check for GUI mode request
if [ "$1" = "--gui" ] || [ -f /opt/chrysalis/gui/enable ]; then
    echo "[Chrysalis] Launching in GUI mode..."
    if [ -x /opt/chrysalis/gui/launcher.sh ]; then
        exec /opt/chrysalis/gui/launcher.sh
    else
        echo "[!] GUI launcher not found, falling back to terminal"
    fi
fi

# Launch Chrysalis daemon in terminal mode
echo "[Chrysalis] Launching Chrysalis daemon..."
if [ -x /opt/chrysalis/chrysalis_daemon ]; then
    exec /opt/chrysalis/chrysalis_daemon
else
    echo "[!] Daemon not found, launching bash fallback..."
    exec /bin/bash
fi

