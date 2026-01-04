// kernel/arch/i386/idt.cpp
#include "idt.h"
#include "../../terminal.h"
#include "../../panic.h"

extern "C" void irq0();
extern "C" void irq1();
extern "C" void isr_default();   // fallback generic ASM handler
extern "C" void syscall_handler();

static IDTEntry idt[256];
static IDTPointer idtp;

/* Public function: expusă prin idt.h */
void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags)
{
    if (num < 0 || num >= 256) {
        panic("idt_set_gate: invalid index");
        return;
    }

    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

/* IDT init + fallback stabil */
extern "C" void idt_init()
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    /* zero table */
    for (int i = 0; i < 256; i++) {
        idt[i].base_low  = 0;
        idt[i].base_high = 0;
        idt[i].sel       = 0;
        idt[i].always0   = 0;
        idt[i].flags     = 0;
    }

    /* setăm un fallback clar pentru toate intrările (stabil) */
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_default, 0x08, 0x8E); // ring0 intr gate
    }

    /* Overwrite cu IRQ-urile tale reale după remap PIC (32/33 etc) */
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
        /* Syscall gate: int 0x80 (ring 3 allowed) */
    idt_set_gate(0x80, (uint32_t)syscall_handler, 0x08, 0xEE);
    idt_set_gate(
    0x80,
    (uint32_t)syscall_handler,
    0x08,      // kernel code segment
    0xEE       // PRESENT | DPL=3 | 32-bit interrupt gate
);


    /* Dacă vei instala syscall gate cu DPL=3, o vei seta altfel (0xEE). */

    /* Load IDT */
    asm volatile("lidt %0" : : "m"(idtp));

    terminal_writestring("[idt] installed (with safe fallback)\n");
}
