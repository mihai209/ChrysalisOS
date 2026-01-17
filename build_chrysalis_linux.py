#!/usr/bin/env python3
"""
Build and integrate ChrysalisOS Linux userspace port with Alpine
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

# Configuration
PROJECT_ROOT = Path("/home/mihai/Desktop/ChrysalisOS")
OS_DIR = PROJECT_ROOT / "os"
LINUX_PORT_DIR = OS_DIR / "linux_port"
HYBRID_DIR = PROJECT_ROOT / "hybrid"
ROOTFS_DIR = HYBRID_DIR / "rootfs"
CHRYSALIS_INSTALL = ROOTFS_DIR / "opt" / "chrysalis"

def run_command(cmd, cwd=None, check=True):
    """Run shell command"""
    print(f"  $ {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    if isinstance(cmd, str):
        result = subprocess.run(cmd, shell=True, cwd=cwd)
    else:
        result = subprocess.run(cmd, cwd=cwd)
    if check and result.returncode != 0:
        print(f"ERROR: Command failed with code {result.returncode}")
        sys.exit(1)
    return result

def stage_1_validate():
    """Validate all required files exist"""
    print("[STAGE 1] Validating build environment...")
    
    required = [
        LINUX_PORT_DIR / "types_linux.h",
        LINUX_PORT_DIR / "syscall_map.h",
        LINUX_PORT_DIR / "chrysalis_daemon.c",
        LINUX_PORT_DIR / "Makefile",
        CHRYSALIS_INSTALL / "init.sh",
    ]
    
    for f in required:
        if not f.exists():
            print(f"ERROR: Missing {f}")
            return False
    
    print("  ✓ All files present")
    return True

def stage_2_compile_daemon():
    """Compile Chrysalis daemon"""
    print("[STAGE 2] Compiling Chrysalis daemon...")
    
    # Create build directory
    (LINUX_PORT_DIR / "build").mkdir(exist_ok=True)
    
    # Compile
    run_command(["make", "clean"], cwd=LINUX_PORT_DIR)
    run_command(["make"], cwd=LINUX_PORT_DIR)
    
    daemon_path = LINUX_PORT_DIR / "chrysalis_daemon"
    if daemon_path.exists():
        print(f"  ✓ Compiled: {daemon_path.stat().st_size} bytes")
        return True
    else:
        print("ERROR: Compilation failed")
        return False

def stage_3_copy_daemon():
    """Copy daemon to rootfs"""
    print("[STAGE 3] Installing Chrysalis daemon...")
    
    daemon_src = LINUX_PORT_DIR / "chrysalis_daemon"
    daemon_dst = CHRYSALIS_INSTALL / "chrysalis_daemon"
    
    if daemon_src.exists():
        shutil.copy2(daemon_src, daemon_dst)
        os.chmod(daemon_dst, 0o755)
        print(f"  ✓ Installed to {daemon_dst}")
        return True
    else:
        print("WARNING: Daemon not found, continuing without")
        return True

def stage_4_copy_headers():
    """Copy compat headers for reference"""
    print("[STAGE 4] Installing compatibility headers...")
    
    # Create include directory
    (CHRYSALIS_INSTALL / "include").mkdir(exist_ok=True)
    
    headers = [
        "types_linux.h",
        "syscall_map.h",
    ]
    
    for header in headers:
        src = LINUX_PORT_DIR / header
        dst = CHRYSALIS_INSTALL / "include" / header
        if src.exists():
            shutil.copy2(src, dst)
            print(f"  ✓ Copied {header}")
    
    return True

def stage_5_create_buildfile():
    """Create Makefile in chrysalis directory for easy rebuilding"""
    print("[STAGE 5] Creating Chrysalis build system...")
    
    makefile = CHRYSALIS_INSTALL / "Makefile"
    makefile.write_text("""# Build Chrysalis OS inside rootfs

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lc -lm

TARGET = chrysalis_daemon
SRCS = ../../../os/linux_port/*.c
OBJS = build/*.o

all: $(TARGET)

$(TARGET):
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)
	@chmod +x $@
	@echo "✓ Built Chrysalis daemon"

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
""")
    print("  ✓ Created build system")
    return True

def stage_6_create_documentation():
    """Create documentation"""
    print("[STAGE 6] Creating documentation...")
    
    readme = CHRYSALIS_INSTALL / "README_LINUX.md"
    readme.write_text("""# Chrysalis OS on Alpine Linux

## Overview

Chrysalis OS now runs as a userspace framework on top of Alpine Linux with Busybox utilities.

## Components

- **Kernel:** Alpine Linux 6.6-virt (real Linux kernel)
- **Shell:** Busybox 1.36.1 (sh/ash)
- **Utilities:** 36+ standard Unix commands
- **Framework:** Chrysalis OS (userspace daemon)

## Running

The system automatically starts `/opt/chrysalis/chrysalis_daemon` on boot, which provides:

- Interactive shell with Chrysalis commands
- Access to all Linux utilities via Busybox
- Full POSIX compatibility

## Rebuilding Daemon

If you modify the daemon source code:

```bash
cd /opt/chrysalis
make clean
make
```

## Architecture

```
Hardware
  ↓
Alpine Linux Kernel (6.6-virt)
  ↓
Busybox + Init System
  ↓
Chrysalis Daemon (userspace)
  ↓
Shell + Commands
  ↓
User
```

## Available Commands

Core shell commands (Chrysalis):
- help, clear, echo, pwd, cd, ls, cat, mkdir, rm
- sysinfo, uptime, uname, exit

System utilities (Busybox):
- All standard Unix tools available

## Development

Chrysalis compatibility layer files:

- `include/types_linux.h` - Type definitions and POSIX compatibility
- `include/syscall_map.h` - Syscall translation layer
- Source: `/os/linux_port/`

To add new commands or features, modify the daemon source and rebuild.
""")
    print("  ✓ Created documentation")
    return True

def stage_7_prepare_rootfs():
    """Prepare rootfs for packaging"""
    print("[STAGE 7] Preparing rootfs...")
    
    # Ensure key directories exist
    (ROOTFS_DIR / "opt" / "chrysalis" / "bin").mkdir(parents=True, exist_ok=True)
    (ROOTFS_DIR / "opt" / "chrysalis" / "include").mkdir(parents=True, exist_ok=True)
    
    # Make init.sh executable
    init_script = CHRYSALIS_INSTALL / "init.sh"
    if init_script.exists():
        os.chmod(init_script, 0o755)
        print(f"  ✓ Updated {init_script}")
    
    print("  ✓ Rootfs ready")
    return True

def stage_8_create_iso():
    """Create new bootable ISO"""
    print("[STAGE 8] Creating bootable ISO...")
    
    # Backup old ISO
    iso_path = HYBRID_DIR / "chrysalis-alpine-hybrid.iso"
    if iso_path.exists():
        backup_path = HYBRID_DIR / "chrysalis-alpine-hybrid.iso.bak"
        shutil.copy2(iso_path, backup_path)
        print(f"  ✓ Backed up old ISO")
    
    # Create new rootfs.tar.gz
    print("  Creating rootfs archive...")
    run_command([
        "tar", "czf", 
        str(HYBRID_DIR / "rootfs.tar.gz"),
        "-C", str(ROOTFS_DIR), "."
    ])
    
    # Rebuild ISO
    print("  Building ISO...")
    run_command([
        "grub-mkrescue",
        "-o", str(iso_path),
        "-d", "/usr/lib/grub/i386-pc",
        "-v",
        str(ROOTFS_DIR)
    ], check=False)
    
    if iso_path.exists():
        size_mb = iso_path.stat().st_size / (1024 * 1024)
        print(f"  ✓ Created: {size_mb:.1f} MB")
        return True
    else:
        print("ERROR: ISO creation failed")
        return False

def stage_9_summary():
    """Print summary"""
    print("")
    print("╔════════════════════════════════════════════════════╗")
    print("║     CHRYSALIS OS LINUX PORT BUILD COMPLETE        ║")
    print("╚════════════════════════════════════════════════════╝")
    print("")
    
    iso_path = HYBRID_DIR / "chrysalis-alpine-hybrid.iso"
    daemon_path = CHRYSALIS_INSTALL / "chrysalis_daemon"
    
    if iso_path.exists():
        size_mb = iso_path.stat().st_size / (1024 * 1024)
        print(f"✓ Bootable ISO: {iso_path} ({size_mb:.1f} MB)")
    
    if daemon_path.exists():
        print(f"✓ Daemon installed: {daemon_path}")
    
    print("")
    print("Next steps:")
    print("  1. Test in QEMU:")
    print(f"     qemu-system-i386 -cdrom {iso_path} -m 512")
    print("")
    print("  2. Select 'Chrysalis OS' from boot menu")
    print("  3. Chrysalis daemon will start automatically")
    print("  4. Try: help, sysinfo, ls, cat, etc.")
    print("")

def main():
    """Main build process"""
    print("")
    print("╔════════════════════════════════════════════════════╗")
    print("║  Building Chrysalis OS Linux Userspace Port       ║")
    print("╚════════════════════════════════════════════════════╝")
    print("")
    
    stages = [
        ("Validation", stage_1_validate),
        ("Compilation", stage_2_compile_daemon),
        ("Installation", stage_3_copy_daemon),
        ("Headers", stage_4_copy_headers),
        ("Build System", stage_5_create_buildfile),
        ("Documentation", stage_6_create_documentation),
        ("Rootfs Prep", stage_7_prepare_rootfs),
        ("ISO Creation", stage_8_create_iso),
        ("Summary", stage_9_summary),
    ]
    
    for name, stage_func in stages:
        try:
            if not stage_func():
                print(f"ERROR: {name} stage failed")
                return 1
        except Exception as e:
            print(f"ERROR: {name} stage failed with exception: {e}")
            return 1
    
    print("\n✨ BUILD SUCCESSFUL\n")
    return 0

if __name__ == "__main__":
    sys.exit(main())
