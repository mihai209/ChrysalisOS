// kernel/drivers/keyboard.cpp
#include <stdint.h>
#include <stdbool.h>
#include "keyboard.h"
#include "../interrupts/irq.h"
#include "../input/keyboard_buffer.h"
#include "../arch/i386/io.h"
#include "../drivers/serial.h"
#include "../input/input.h"
#include "../drivers/pic.h"
#include "../hardware/lapic.h"

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
static bool ctrl_pressed = false;
static bool shift_pressed = false;
static bool alt_pressed = false;

extern "C" void keyboard_handler(registers_t* regs)
{
    (void)regs;

    /* Read status register */
    uint8_t status = inb(0x64);

    /* Check if output buffer is full (bit 0) */
    if (status & 0x01) {
        uint8_t scancode = inb(0x60);

        /* If USB Keyboard is active, ignore PS/2 to prevent conflicts */
        /* --- KEY RELEASES --- */
        if (scancode & 0x80) {
            uint8_t sc = scancode & 0x7F;

            /* Shift release */
            if (sc == 0x2A || sc == 0x36) {
                shift_pressed = false;
            }
            /* Ctrl release */
            else if (sc == 0x1D) {
                ctrl_pressed = false;
            }
            /* Alt release */
            else if (sc == 0x38) {
                alt_pressed = false;
            }
        } else {
            /* --- KEY PRESSES --- */

            /* Shift press */
            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = true;
            }
            /* Ctrl press */
            else if (scancode == 0x1D) {
                ctrl_pressed = true;
            }
            /* Alt press */
            else if (scancode == 0x38) {
                alt_pressed = true;
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

                if (c) {
                    /* Push to legacy buffer */
                    // kbd_push(c);
                    /* Push to new input system */
                    input_push_key((uint32_t)c, true);
                }
            }
        }
    }

}

extern "C" void keyboard_init()
{
    serial_write_string("[PS/2] Initializing keyboard driver...\r\n");

    /* 1. Disable Keyboard Port */
    outb(0x64, 0xAD);

    /* 2. Flush Output Buffer */
    while(inb(0x64) & 1) inb(0x60);

    /* 3. Enable Keyboard Port */
    outb(0x64, 0xAE);

    /* Reset keyboard */
    outb(0x60, 0xFF);
    
    /* Wait for ACK (0xFA) */
    int timeout = 100000;
    while(timeout-- && !(inb(0x64) & 1));
    if (timeout > 0) {
        uint8_t resp = inb(0x60);
        serial_printf("[PS/2] reset resp=0x%x\n", resp);
    }

    /* Wait for BAT (0xAA) - Self Test Passed */
    timeout = 1000000; 
    while(timeout-- > 0) {
        if (inb(0x64) & 1) {
            uint8_t bat = inb(0x60);
            serial_printf("[PS/2] BAT/Extra: 0x%x\n", bat);
            if (bat == 0xAA) break; // Found BAT, we are good
        }
        asm volatile("pause");
    }

    /* Enable scanning */
    outb(0x60, 0xF4);
    while (!(inb(0x64) & 1));
    inb(0x60);

    /* 4. Enable Interrupts in Command Byte */
    outb(0x64, 0x20); // Read Config
    while ((inb(0x64) & 1) == 0); // Wait for data
    uint8_t cmd = inb(0x60);
    cmd |= 0x01; // Enable IRQ1
    outb(0x64, 0x60); // Write Config
    while ((inb(0x64) & 2)); // Wait for input buffer empty
    outb(0x60, cmd);

    irq_install_handler(1, keyboard_handler);
    
    /* 5. Unmask IRQ1 in PIC (Master) explicitly */
    uint8_t mask = inb(0x21);
    outb(0x21, mask & 0xFD); // Clear bit 1 (IRQ1)

    serial_write_string("[PS/2] Keyboard handler installed on IRQ1.\r\n");
    input_signal_ready();
}
