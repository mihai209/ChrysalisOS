#include <stdint.h>
#include "keyboard.h"
#include "../interrupts/irq.h"
#include "../terminal.h"
#include "../shell/shell.h"

static const char keymap[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' '
};

static void keyboard_irq(registers_t*) {
    uint8_t scancode;
    asm volatile("inb %1, %0" : "=a"(scancode) : "Nd"(0x60));

    if (scancode & 0x80)
        return;

    char c = keymap[scancode];
    if (c)
        shell_handle_char(c);
}

void keyboard_init() {
    irq_install_handler(1, keyboard_irq);
}
