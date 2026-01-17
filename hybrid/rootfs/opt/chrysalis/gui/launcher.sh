#!/bin/sh
# Chrysalis OS GUI Launcher

# Set environment variables
export DISPLAY=:0
export HOME=/root
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/opt/chrysalis/bin:$PATH
export SHELL=/bin/bash

# Create minimal X11 directories if needed
mkdir -p /tmp/.X11-unix

# Create xinitrc for minimal X session
cat > /root/.xinitrc << 'XINITRC'
#!/bin/bash
# Minimal X session for Chrysalis OS
xsetroot -solid "#2c3e50"

# Start a simple window manager (if available) or just run bash
if command -v fluxbox >/dev/null 2>&1; then
    fluxbox &
elif command -v jwm >/dev/null 2>&1; then
    jwm &
else
    # Fallback: just show an X terminal
    xterm -bg "#2c3e50" -fg "#ecf0f1" &
fi

# Keep X session running
wait
XINITRC

chmod +x /root/.xinitrc

# Start Chrysalis GUI daemon
/opt/chrysalis/chrysalis_daemon --gui 2>&1 | tee -a /var/log/chrysalis.log

