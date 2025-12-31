#include "idt.h"

extern "C" void irq0();
extern "C" void irq1();

static IDTEntry idt[256];
static IDTPointer idtp;

static void set_gate(int n, uint32_t handler)
{
    idt[n].base_low = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].always0 = 0;
    idt[n].flags = 0x8E;
}

extern "C" void idt_init()
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;

    for (int i = 0; i < 256; i++)
        set_gate(i, 0);

    set_gate(32, (uint32_t)irq0);
    set_gate(33, (uint32_t)irq1);

    asm volatile("lidt %0" : : "m"(idtp));
}
