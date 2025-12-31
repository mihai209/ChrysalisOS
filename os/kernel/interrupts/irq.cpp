#include "../drivers/pic.h"
#include "../terminal.h"

extern "C" void irq_handler(int irq)
{
    if (irq == 1) {
        terminal_writestring("KEY\n");
    }

    pic_send_eoi(irq);
}
