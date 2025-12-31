#include "reboot.h"

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

extern "C" void cmd_reboot(const char*) {
    // Disable interrupts
    asm volatile("cli");

    // Wait until keyboard controller is ready
    while (inb(0x64) & 0x02);

    // Pulse CPU reset line
    outb(0x64, 0xFE);

    // Fallback: triple fault
    asm volatile(
        "lidt (%0)\n"
        "int $3\n"
        :
        : "r"(0)
    );

    for (;;);
}
