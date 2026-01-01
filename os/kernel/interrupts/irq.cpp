#include "irq.h"
#include "../arch/i386/io.h"
#include "../drivers/pic.h"

static irq_handler_t irq_routines[16] = {0};

void irq_install_handler(int irq, irq_handler_t handler) {
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    irq_routines[irq] = 0;
}

extern "C" void irq_handler(registers_t* regs) {
    int irq = regs->int_no - 32;

    if (irq < 16 && irq_routines[irq]) {
        irq_routines[irq](regs);
    }

    /* End Of Interrupt */
    if (irq >= 8)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
