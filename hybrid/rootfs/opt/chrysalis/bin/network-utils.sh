#!/bin/bash
# Chrysalis OS Network Utilities

# Show this help
help() {
    cat << 'NETHELP'
Chrysalis OS Network Commands:

  network-check   - Check network connectivity
  network-config  - Configure network interface
  network-status  - Show network status
  download FILE   - Download file (uses curl/wget)
  
Examples:
  network-check
  network-status
  download http://example.com/file.zip
NETHELP
}

network-check() {
    echo "Checking network connectivity..."
    for server in 8.8.8.8 1.1.1.1 208.67.222.222; do
        if ping -c 1 -W 1 $server >/dev/null 2>&1; then
            echo "✓ Connected to $server"
            return 0
        fi
    done
    echo "✗ Network not reachable"
    return 1
}

network-status() {
    echo "=== Network Status ==="
    echo "Interfaces:"
    ip addr show 2>/dev/null || ifconfig 2>/dev/null || echo "ip/ifconfig not available"
    echo ""
    echo "Routes:"
    ip route show 2>/dev/null || route -n 2>/dev/null || echo "route info not available"
    echo ""
    echo "DNS:"
    cat /etc/resolv.conf 2>/dev/null || echo "resolv.conf not available"
}

network-config() {
    echo "Configuring eth0 with DHCP..."
    udhcpc -i eth0
}

download() {
    if [ -z "$1" ]; then
        echo "Usage: download <URL>"
        return 1
    fi
    
    filename=$(basename "$1")
    echo "Downloading: $1"
    
    if command -v curl >/dev/null 2>&1; then
        curl -L -o "$filename" "$1"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$filename" "$1"
    else
        echo "Error: curl or wget not available"
        return 1
    fi
    
    if [ -f "$filename" ]; then
        echo "✓ Downloaded: $filename ($(du -h $filename | cut -f1))"
    else
        echo "✗ Download failed"
        return 1
    fi
}

# Main
case "$1" in
    help|--help|-h)
        help
        ;;
    check)
        network-check
        ;;
    status)
        network-status
        ;;
    config)
        network-config
        ;;
    download)
        download "$2"
        ;;
    *)
        help
        ;;
esac

