// Chrysalis OS - Kernel Entry Point
// This is where the kernel starts executing

#include "../include/terminal.h"

extern "C" void kernel_main() {
    // Initialize terminal
    terminal_initialize();
    
    // Clear screen and print welcome message
    terminal_clear();
    terminal_writestring("Welcome to Chrysalis OS!\n");
    terminal_writestring("Version: 0.1.0 (Terminal Mode)\n");
    terminal_writestring("\n");
    terminal_writestring("System initialized successfully.\n");
    terminal_writestring("Kernel loaded and running...\n");
    
    // Halt - we'll add more functionality later
    while(1) {
        asm volatile("hlt");
    }
}
