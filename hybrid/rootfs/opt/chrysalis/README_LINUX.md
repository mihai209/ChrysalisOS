# Chrysalis OS on Alpine Linux

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
