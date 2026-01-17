#!/bin/bash
# Chrysalis OS bash configuration

export PS1="\[\033[01;32m\]chrysalis\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]$ "
export TERM=xterm-256color
export HISTSIZE=1000
export HISTFILESIZE=2000
export EDITOR=vi

# Aliases
alias ll='ls -la'
alias la='ls -A'
alias l='ls -CF'
alias clear='clear; echo "Chrysalis OS - Interactive Shell"'

# Functions
motd() {
    echo "╔════════════════════════════════════╗"
    echo "║  Chrysalis OS on Alpine Linux      ║"
    echo "║  Kernel: Alpine 6.6-virt          ║"
    echo "║  Shell: bash                       ║"
    echo "║  GUI: Enabled                      ║"
    echo "║  Network: Enabled (curl, wget)     ║"
    echo "╚════════════════════════════════════╝"
}

# Show motd on login
motd

# Network check
check_network() {
    if ping -c 1 8.8.8.8 >/dev/null 2>&1; then
        echo "✓ Network: Connected"
    else
        echo "✗ Network: Disconnected"
    fi
}

# Print current system info
sysinfo_bash() {
    echo ""
    echo "=== Chrysalis OS System Information ==="
    uname -a
    echo "CPU: $(grep -c processor /proc/cpuinfo) cores"
    echo "Memory: $(free -h | grep Mem | awk '{print $2}')"
    echo "Uptime: $(uptime -p 2>/dev/null || uptime)"
    check_network
    echo ""
}

sysinfo_bash

