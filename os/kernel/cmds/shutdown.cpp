#include "shutdown.h"

static inline void outw(unsigned short port, unsigned short val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

extern "C" void cmd_shutdown(const char*) {
    asm volatile("cli");

    // QEMU / Bochs / modern emulators
    outw(0x604, 0x2000);

    // Older QEMU
    outw(0xB004, 0x2000);

    // VirtualBox
    outw(0x4004, 0x3400);

    // ACPI fallback (some BIOSes)
    outb(0xF4, 0x00);

    // If everything failed → halt CPU forever
    for (;;) {
        asm volatile("hlt");
    }
}
