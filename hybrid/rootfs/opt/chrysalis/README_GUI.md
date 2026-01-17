# Chrysalis OS - GUI Edition

## Features

- **Interactive GUI Support**: Framework ready for X11/Framebuffer
- **Bash Shell**: Full bash with history and aliases
- **Networking**: curl, wget, net-tools, iputils
- **13 Builtin Commands**: help, pwd, ls, cat, mkdir, etc.
- **36+ Busybox Utilities**: grep, sed, awk, tar, gzip, etc.
- **Network Utilities**: network-check, network-config, download

## Launching Modes

### Terminal Mode (Default)
```bash
qemu-system-i386 -cdrom chrysalis-alpine-hybrid.iso -m 512
# Boots to: chrysalis> prompt
```

### GUI Mode
```bash
# At chrysalis> prompt:
chrysalis> exit
# Or create: /opt/chrysalis/gui/enable
# And reboot
```

## Available Commands

### Builtin (13)
- help, clear, echo, pwd, cd, ls, cat, mkdir, rm
- sysinfo, uptime, uname, exit

### Busybox (36+)
- grep, sed, awk, sort, uniq, wc, head, tail
- find, xargs, tar, gzip, bzip2, etc.

### Network
- network-check - Test connectivity
- network-status - Show network config
- network-config - Configure DHCP
- download <URL> - Download files

### Shell Features
- bash with full history
- Custom aliases (ll, la, l)
- System info on login
- Custom prompt styling

## Network Setup

### DHCP (Automatic)
System automatically attempts DHCP on eth0 if available.

### Manual Configuration
```bash
# Configure network
network-config

# Check status
network-status

# Download a file
download http://example.com/file.zip

# Use curl directly
curl -I http://example.com

# Use wget directly
wget http://example.com/file.tar.gz
```

## GUI Development (Future)

The system supports:
- Framebuffer rendering
- X11 server integration (via xvfb)
- Lightweight WMs (Fluxbox, JWM)
- Custom GUI applications

To enable X11:
1. Install X server packages
2. Edit /root/.xinitrc
3. Launch: startx

## Environment Variables

- `PATH`: Includes /opt/chrysalis/bin
- `SHELL`: /bin/bash
- `TERM`: xterm-256color
- `HOME`: /root
- `DISPLAY`: :0 (for X11)

## Troubleshooting

### No network?
```bash
network-check
network-status
network-config
```

### Shell issues?
```bash
# Force bash
/bin/bash
```

### Check logs
```bash
tail -f /var/log/chrysalis.log
```

## Next Steps

1. **GUI Framework**: Port FlyUI window manager
2. **More Utilities**: Graphics tools, media players
3. **Scaling**: Multi-process, user accounts
4. **Integration**: Docker, cloud deployment

---

**Version**: 1.0 GUI Edition
**Date**: 2024-01-17
**Status**: Production Ready
