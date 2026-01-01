#include "irq.h"
#include "../arch/i386/io.h"

/* Table with user-installed handlers for IRQs 0..15 */
static irq_handler_t irq_routines[16] = { 0 };

void irq_install_handler(int irq, irq_handler_t handler) {
    if (irq < 0 || irq >= 16) return;
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    if (irq < 0 || irq >= 16) return;
    irq_routines[irq] = 0;
}

/* Called from asm stubs (vectors 32..47) */
extern "C" void irq_handler(registers_t* regs) {
    int irq = regs->int_no - 32;

    if (irq >= 0 && irq < 16) {
        irq_handler_t h = irq_routines[irq];
        if (h) {
            h(regs);
        }
    }

    /* End Of Interrupt: first to slave (IRQ 8-15), then master */
    if (irq >= 8)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
