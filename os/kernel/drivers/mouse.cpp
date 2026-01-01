#include "mouse.h"
#include "../interrupts/irq.h"

static uint8_t packet[3];
static uint8_t packet_index = 0;

/* stare mouse */
static int mouse_dx = 0;
static int mouse_dy = 0;
static uint8_t mouse_buttons = 0;

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void mouse_handler(registers_t*)
{
    uint8_t data = inb(0x60);

    packet[packet_index++] = data;

    if (packet_index < 3)
        return;

    packet_index = 0;

    mouse_buttons = packet[0] & 0x07;

    mouse_dx += (int8_t)packet[1];
    mouse_dy -= (int8_t)packet[2]; // Y invers
}

extern "C" void mouse_init()
{
    /* enable IRQ12 */
    outb(0xA1, inb(0xA1) & ~(1 << 4));

    /* enable auxiliary device */
    outb(0x64, 0xA8);

    /* enable interrupts */
    outb(0x64, 0x20);
    uint8_t status = inb(0x60);
    status |= 2;
    outb(0x64, 0x60);
    outb(0x60, status);

    /* default settings */
    outb(0x64, 0xD4);
    outb(0x60, 0xF6);
    inb(0x60);

    /* enable streaming */
    outb(0x64, 0xD4);
    outb(0x60, 0xF4);
    inb(0x60);

    irq_install_handler(12, mouse_handler);
}
