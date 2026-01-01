#include "speaker.h"
#include "../arch/i386/io.h"
#include "../drivers/pit.h"

static void speaker_enable()
{
    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3)
        outb(0x61, tmp | 3);
}

static void speaker_disable()
{
    outb(0x61, inb(0x61) & ~3);
}

void speaker_on(uint32_t freq)
{
    uint32_t div = 1193180 / freq;

    outb(0x43, 0xB6);
    outb(0x42, div & 0xFF);
    outb(0x42, (div >> 8) & 0xFF);

    speaker_enable();
}

void speaker_off()
{
    speaker_disable();
}

void speaker_beep(uint32_t freq, uint32_t ms)
{
    speaker_on(freq);
    pit_sleep(ms);
    speaker_off();
}
