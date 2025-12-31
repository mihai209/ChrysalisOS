#!/bin/bash

# Chrysalis OS Development Environment Setup Script
# Detects OS and manages package installation/removal

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Package groups
CORE_PACKAGES=("build-essential" "nasm" "gcc" "g++" "make" "binutils" "gdb")
QEMU_PACKAGES=("qemu-system-x86" "qemu-utils")
BOOTLOADER_PACKAGES=("grub-pc-bin" "xorriso" "mtools")
OPTIONAL_PACKAGES=("git" "cmake" "clang" "lldb" "valgrind")

# Detect OS
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        VERSION=$VERSION_ID
    else
        echo -e "${RED}Cannot detect OS. /etc/os-release not found.${NC}"
        exit 1
    fi
}

# Print banner
print_banner() {
    echo -e "${BLUE}"
    echo "╔═══════════════════════════════════════════════╗"
    echo "║   Chrysalis OS - Development Environment      ║"
    echo "║           Package Manager Script              ║"
    echo "╚═══════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# Check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}This script requires sudo privileges.${NC}"
        echo "Please run with sudo or as root."
        exit 1
    fi
}

# Display detected OS
display_os_info() {
    echo -e "${GREEN}Detected OS: ${NC}$OS"
    echo -e "${GREEN}Version: ${NC}$VERSION"
    echo ""
}

# Convert package names based on OS
convert_package_name() {
    local pkg=$1
    case $OS in
        debian|ubuntu|linuxmint)
            echo "$pkg"
            ;;
        arch|manjaro)
            case $pkg in
                "build-essential") echo "base-devel" ;;
                "qemu-system-x86") echo "qemu-full" ;;
                "qemu-utils") echo "" ;;  # Included in qemu-full
                "grub-pc-bin") echo "grub" ;;
                *) echo "$pkg" ;;
            esac
            ;;
        fedora|rhel|centos)
            case $pkg in
                "build-essential") echo "gcc gcc-c++ make" ;;
                "qemu-system-x86") echo "qemu" ;;
                "qemu-utils") echo "" ;;
                "grub-pc-bin") echo "grub2-tools" ;;
                "g++") echo "gcc-c++" ;;
                *) echo "$pkg" ;;
            esac
            ;;
        *)
            echo "$pkg"
            ;;
    esac
}

# Install packages
install_packages() {
    local packages=("$@")
    local converted_packages=()
    
    for pkg in "${packages[@]}"; do
        converted=$(convert_package_name "$pkg")
        if [ -n "$converted" ]; then
            converted_packages+=($converted)
        fi
    done
    
    if [ ${#converted_packages[@]} -eq 0 ]; then
        echo -e "${YELLOW}No packages to install.${NC}"
        return
    fi
    
    echo -e "${BLUE}Installing: ${converted_packages[*]}${NC}"
    
    case $OS in
        debian|ubuntu|linuxmint)
            apt update
            apt install -y "${converted_packages[@]}"
            ;;
        arch|manjaro)
            pacman -Sy --noconfirm "${converted_packages[@]}"
            ;;
        fedora|rhel|centos)
            dnf install -y "${converted_packages[@]}"
            ;;
        *)
            echo -e "${RED}Unsupported OS: $OS${NC}"
            exit 1
            ;;
    esac
    
    echo -e "${GREEN}Installation complete!${NC}"
}

# Remove packages
remove_packages() {
    local packages=("$@")
    local converted_packages=()
    
    for pkg in "${packages[@]}"; do
        converted=$(convert_package_name "$pkg")
        if [ -n "$converted" ]; then
            converted_packages+=($converted)
        fi
    done
    
    if [ ${#converted_packages[@]} -eq 0 ]; then
        echo -e "${YELLOW}No packages to remove.${NC}"
        return
    fi
    
    echo -e "${RED}Removing: ${converted_packages[*]}${NC}"
    
    case $OS in
        debian|ubuntu|linuxmint)
            apt remove -y "${converted_packages[@]}"
            apt autoremove -y
            ;;
        arch|manjaro)
            pacman -Rns --noconfirm "${converted_packages[@]}"
            ;;
        fedora|rhel|centos)
            dnf remove -y "${converted_packages[@]}"
            ;;
        *)
            echo -e "${RED}Unsupported OS: $OS${NC}"
            exit 1
            ;;
    esac
    
    echo -e "${GREEN}Removal complete!${NC}"
}

# Display package group menu
display_package_menu() {
    echo -e "${YELLOW}Available Package Groups:${NC}"
    echo "1) Core Development Tools (Essential)"
    echo "   - ${CORE_PACKAGES[*]}"
    echo ""
    echo "2) QEMU Emulator"
    echo "   - ${QEMU_PACKAGES[*]}"
    echo ""
    echo "3) Bootloader Tools"
    echo "   - ${BOOTLOADER_PACKAGES[*]}"
    echo ""
    echo "4) Optional Tools"
    echo "   - ${OPTIONAL_PACKAGES[*]}"
    echo ""
    echo "5) Install ALL packages"
    echo "6) Remove packages"
    echo "0) Exit"
    echo ""
}

# Main menu
main_menu() {
    while true; do
        display_package_menu
        read -p "Select an option (0-6): " choice
        
        case $choice in
            1)
                echo -e "${GREEN}Installing Core Development Tools...${NC}"
                install_packages "${CORE_PACKAGES[@]}"
                ;;
            2)
                echo -e "${GREEN}Installing QEMU Emulator...${NC}"
                install_packages "${QEMU_PACKAGES[@]}"
                ;;
            3)
                echo -e "${GREEN}Installing Bootloader Tools...${NC}"
                install_packages "${BOOTLOADER_PACKAGES[@]}"
                ;;
            4)
                echo -e "${GREEN}Installing Optional Tools...${NC}"
                install_packages "${OPTIONAL_PACKAGES[@]}"
                ;;
            5)
                echo -e "${GREEN}Installing ALL packages...${NC}"
                install_packages "${CORE_PACKAGES[@]}" "${QEMU_PACKAGES[@]}" "${BOOTLOADER_PACKAGES[@]}" "${OPTIONAL_PACKAGES[@]}"
                ;;
            6)
                removal_menu
                ;;
            0)
                echo -e "${BLUE}Exiting. Happy OS development!${NC}"
                exit 0
                ;;
            *)
                echo -e "${RED}Invalid option. Please try again.${NC}"
                ;;
        esac
        echo ""
        read -p "Press Enter to continue..."
        clear
        print_banner
        display_os_info
    done
}

# Removal menu
removal_menu() {
    echo -e "${RED}Package Removal Menu${NC}"
    echo "1) Remove Core Development Tools"
    echo "2) Remove QEMU Emulator"
    echo "3) Remove Bootloader Tools"
    echo "4) Remove Optional Tools"
    echo "5) Remove ALL packages"
    echo "0) Back to main menu"
    echo ""
    read -p "Select an option (0-5): " choice
    
    case $choice in
        1)
            read -p "Are you sure? (y/N): " confirm
            if [[ $confirm == [yY] ]]; then
                remove_packages "${CORE_PACKAGES[@]}"
            fi
            ;;
        2)
            read -p "Are you sure? (y/N): " confirm
            if [[ $confirm == [yY] ]]; then
                remove_packages "${QEMU_PACKAGES[@]}"
            fi
            ;;
        3)
            read -p "Are you sure? (y/N): " confirm
            if [[ $confirm == [yY] ]]; then
                remove_packages "${BOOTLOADER_PACKAGES[@]}"
            fi
            ;;
        4)
            read -p "Are you sure? (y/N): " confirm
            if [[ $confirm == [yY] ]]; then
                remove_packages "${OPTIONAL_PACKAGES[@]}"
            fi
            ;;
        5)
            read -p "Remove ALL packages? This cannot be undone! (y/N): " confirm
            if [[ $confirm == [yY] ]]; then
                remove_packages "${CORE_PACKAGES[@]}" "${QEMU_PACKAGES[@]}" "${BOOTLOADER_PACKAGES[@]}" "${OPTIONAL_PACKAGES[@]}"
            fi
            ;;
        0)
            return
            ;;
        *)
            echo -e "${RED}Invalid option.${NC}"
            ;;
    esac
}

# Main execution
main() {
    print_banner
    check_root
    detect_os
    display_os_info
    main_menu
}

main