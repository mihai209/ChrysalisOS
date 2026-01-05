// kernel/drivers/keyboard.cpp
#include <stdint.h>
#include <stdbool.h>
#include "keyboard.h"
#include "../interrupts/irq.h"
#include "../input/keyboard_buffer.h"
#include "../arch/i386/io.h"
#include "../drivers/serial.h"

/* US QWERTY Keymap (embedded for stability) */
static const char keymap_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char keymap_us_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* state flags */
static int ctrl_pressed = 0;
static int shift_pressed = 0;
static int alt_pressed = 0;

extern "C" void keyboard_handler(registers_t* regs)
{
    (void)regs;

    uint8_t scancode = inb(0x60);

    /* --- KEY RELEASES --- */
    if (scancode & 0x80) {
        uint8_t sc = scancode & 0x7F;

        /* Shift release */
        if (sc == 0x2A || sc == 0x36) {
            shift_pressed = 0;
        }
        /* Ctrl release (0x1D + 0x80 = 0x9D) */
        else if (sc == 0x1D) {
            ctrl_pressed = 0;
        }
        /* Alt release */
        else if (sc == 0x38) {
            alt_pressed = 0;
        }
    } else {
        /* --- KEY PRESSES --- */

        /* Shift press */
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
        }
        /* Ctrl press */
        else if (scancode == 0x1D) {
            ctrl_pressed = 1;
        }
        /* Alt press */
        else if (scancode == 0x38) {
            alt_pressed = 1;
        }
        else {
            /* Translate scancode -> char */
            char c = 0;
            if (scancode < 128) {
                if (shift_pressed) {
                    c = keymap_us_shift[scancode];
                } else {
                    c = keymap_us[scancode];
                }
            }

            /* Handle Ctrl+C, etc. if needed here, or just push char */
            /* For now, just push to buffer so shell gets it */
            if (c) {
                kbd_push(c);
            }
        }
    }
}

extern "C" void keyboard_init()
{
    serial_write_string("[PS/2] Initializing keyboard driver...\r\n");
    irq_install_handler(1, keyboard_handler);
    serial_write_string("[PS/2] Keyboard handler installed on IRQ1.\r\n");
}
