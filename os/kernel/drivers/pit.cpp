#include "pit.h"
#include "../interrupts/irq.h"
#include "../arch/i386/io.h"
#include "../hardware/lapic.h"

static volatile uint64_t pit_ticks = 0;

/* IRQ0 handler */
static void pit_callback(registers_t*)
{
    pit_ticks++;
}

void pit_init(uint32_t freq)
{
    uint32_t divisor = 1193180 / freq;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);

    irq_install_handler(0, pit_callback);
}

uint64_t pit_get_ticks()
{
    return pit_ticks;
}

void pit_sleep(uint32_t ms)
{
    uint64_t start = pit_ticks;

    while ((pit_ticks - start) < ms) {
        asm volatile("hlt");
    }
}
