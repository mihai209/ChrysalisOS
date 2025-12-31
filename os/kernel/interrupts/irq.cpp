#include "irq.h"
#include "../../include/ports.h"
#include "../drivers/keyboard.h"

extern "C" void irq_handler() {
    uint8_t scancode = inb(0x60);

    keyboard_handle(scancode);

    // End Of Interrupt către PIC
    outb(0x20, 0x20);
}
