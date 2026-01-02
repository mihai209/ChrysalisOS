// kernel/time/timer.c
#include "timer.h"
#include "../drivers/pit.h"
#include "../interrupts/irq.h"
#include "../interrupts/isr.h"
#include <stdint.h>

/* PIC EOI helper (master PIC).
   Castăm explicit la tipuri mici pentru a evita ca assembler-ul să folosească %eax. */
static inline void pic_send_eoi(void) {
    uint8_t val = 0x20;
    uint16_t port = 0x20;
    /* AT&T: outb %al, %dx  OR  outb %al, $imm8
       Using constraints: "a"(val) -> %al, "Nd"(port) -> imm or %dx.
       Make sure val/port are typed so compiler picks 8-bit register. */
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* tick counter (updated in IRQ context) */
static volatile uint64_t ticks = 0;
static uint32_t tick_hz = 100; /* default if uninitialized */

/* IRQ handler signature MUST match irq_handler_t (registers_t*) */
static void timer_irq_handler(registers_t* regs)
{
    (void)regs;
    ticks++;

    /* send EOI to PIC (master only for IRQ0) */
    pic_send_eoi();
}

/* initialize timer abstraction */
void timer_init(uint32_t frequency)
{
    if (frequency == 0) return;

    tick_hz = frequency;
    ticks = 0;

    /* initialize PIT hardware to desired frequency */
    pit_init(frequency);

    /* install IRQ handler for IRQ0 (timer) */
    irq_install_handler(0, timer_irq_handler);
}

uint64_t timer_ticks(void)
{
    return ticks;
}

/* sleep(milliseconds) — waits using tick counter; uses HLT to idle */
void sleep(uint32_t ms)
{
    if (tick_hz == 0) return;

    /* ticks = (ms * hz) / 1000, dar fără 64-bit division */
    uint32_t wait_ticks = (ms * tick_hz) / 1000;
    if (wait_ticks == 0)
        wait_ticks = 1;

    uint64_t start = ticks;

    while ((ticks - start) < wait_ticks) {
        asm volatile("hlt");
    }
}

