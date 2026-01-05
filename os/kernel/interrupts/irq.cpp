#include "irq.h"
#include "isr.h"
#include "../arch/i386/io.h" // Asigură-te că ai outb definit aici
#include "../arch/i386/idt.h" // Necesar pentru idt_set_gate
#include <stddef.h>
#include "../hardware/apic.h"
#include "../hardware/lapic.h"

extern "C" {
    void *irq_routines[16] = { 0 };
}

/* Declarații externe pentru stub-urile ASM din irq_stubs.S */
extern "C" {
    void irq0();
    void irq1();
    void irq2();
    void irq3();
    void irq4();
    void irq5();
    void irq6();
    void irq7();
    void irq8();
    void irq9();
    void irq10();
    void irq11();
    void irq12();
    void irq13();
    void irq14();
    void irq15();
}

/* Handler implicit sigur: nu face nimic, doar permite trimiterea EOI.
   Previne crash-urile cauzate de IRQ-uri neașteptate sau race-conditions la boot. */
static void irq_default_stub(registers_t *r)
{
    (void)r;
}

/* Instalează stub-urile IRQ în IDT (intrările 32-47) */
extern "C" void irq_install(void)
{
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    /* Inițializăm toate sloturile cu stub-ul implicit */
    for (int i = 0; i < 16; i++) {
        irq_routines[i] = (void*)irq_default_stub;
    }
}

extern "C" void irq_install_handler(int irq, void (*handler)(registers_t *r))
{
    irq_routines[irq] = (void*)handler;
}

extern "C" void irq_uninstall_handler(int irq)
{
    irq_routines[irq] = 0;
}

extern "C" void irq_handler(registers_t *r)
{
    void (*handler)(registers_t *r);

    // Verificare de siguranță
    if (r->int_no < 32 || r->int_no > 47) return;

    int irq_no = r->int_no - 32;
    
    handler = (void (*)(registers_t *))irq_routines[irq_no];
    
    if (handler)
    {
        handler(r);
    }

    // EOI Logic - Trimis DOAR aici
    if (apic_is_active()) {
        lapic_eoi();
    } else {
        // PIC EOI
        if (irq_no >= 8)
        {
            outb(0xA0, 0x20); // EOI Slave
        }
        outb(0x20, 0x20);     // EOI Master
    }
}
