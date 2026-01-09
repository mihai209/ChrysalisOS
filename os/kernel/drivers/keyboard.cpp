// kernel/drivers/keyboard.cpp
#include <stdint.h>
#include <stdbool.h>
#include "keyboard.h"
#include "../interrupts/irq.h"
#include "../arch/i386/io.h"
#include "../drivers/serial.h"
#include "../input/input.h"
#include "../video/fb_console.h"
#include "mouse.h" /* Pentru mouse_init la reload */
#include "../vt/vt.h"

/* PS/2 Ports */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

/* Import serial logging from kernel glue */
extern "C" void serial(const char *fmt, ...);

/* Status Register Bits */
#define STATUS_OUTPUT_FULL  0x01
#define STATUS_INPUT_FULL   0x02
#define STATUS_SYSTEM       0x04
#define STATUS_CMD_DATA     0x08    /* 0 = Data, 1 = Command */
#define STATUS_KEYLOCK      0x10
#define STATUS_AUX_FULL     0x20    /* 1 = Mouse data */
#define STATUS_TIMEOUT      0x40
#define STATUS_PARITY       0x80

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
static bool e0_prefix = false;

/* Helpers pentru sincronizare PS/2 */
static void kbd_wait_write() {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS) & STATUS_INPUT_FULL) == 0) return;
        asm volatile("pause");
    }
}

static void kbd_wait_read() {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS) & STATUS_OUTPUT_FULL) == 1) return;
        asm volatile("pause");
    }
}

extern "C" void keyboard_handler(registers_t* regs)
{
    (void)regs;
    
    /* Safety limit to prevent infinite loops in ISR */
    int loops = 0;
    const int max_loops = 100;

    /* Loop to handle all pending data (robustness against IRQ sharing/latency) */
    while (loops < max_loops) {
        uint8_t status = inb(PS2_STATUS);

        /* If output buffer is empty, we are done */
        if (!(status & STATUS_OUTPUT_FULL)) {
            break;
        }

        /* If data is from Mouse (Aux) but triggered IRQ1, we MUST read it to clear
           the controller, otherwise the system freezes waiting for a read that never happens.
           We discard it to keep the keyboard alive. */
        if (status & STATUS_AUX_FULL) {
            uint8_t mdata = inb(PS2_DATA);
            serial("[KBD] Warn: Mouse data on IRQ1 (0x%x), discarded to unblock.\n", mdata);
            continue;
        }

        /* Read Scancode */
        uint8_t scancode = inb(PS2_DATA);

        /* If USB keyboard is active, ignore PS/2 to avoid duplicates */
        if (input_is_usb_keyboard_active()) {
            continue;
        }

        /* Handle Extended Scancodes (E0) */
        if (scancode == 0xE0) {
            e0_prefix = true;
            continue;
        }

        /* Handle Break Codes (Key Release) */
        if (scancode & 0x80) {
            uint8_t sc = scancode & 0x7F;
            if (sc == 0x2A || sc == 0x36) shift_pressed = false;
            else if (sc == 0x1D) ctrl_pressed = false;
            else if (sc == 0x38) alt_pressed = false;
            
            e0_prefix = false; 
        } 
        /* Handle Make Codes (Key Press) */
        else {
            if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
            else if (scancode == 0x1D) ctrl_pressed = true;
            else if (scancode == 0x38) alt_pressed = true;
            else {
                /* VT Switching: Ctrl + Alt + F1..F8 */
                if (ctrl_pressed && alt_pressed && scancode >= 0x3B && scancode <= 0x42) {
                    int vt_idx = scancode - 0x3B;
                    vt_switch(vt_idx);
                    return;
                }

                /* Handle Special Keys with E0 prefix */
                if (e0_prefix) {
                    if (scancode == 0x49) fb_cons_scroll(-10);      /* Page Up */
                    else if (scancode == 0x51) fb_cons_scroll(10);  /* Page Down */
                    else if (scancode == 0x48) fb_cons_scroll(-1);  /* Arrow Up */
                    else if (scancode == 0x50) fb_cons_scroll(1);   /* Arrow Down */
                    
                    e0_prefix = false;
                } 
                /* Standard Keys */
                else {
                    /* Keypad Scroll Fallback */
                    if (scancode == 0x48) { fb_cons_scroll(-1); }      /* Keypad 8 */
                    else if (scancode == 0x50) { fb_cons_scroll(1); }  /* Keypad 2 */
                    else {
                        /* ASCII Translation */
                        char c = 0;
                        if (scancode < 128) {
                            c = shift_pressed ? keymap_us_shift[scancode] : keymap_us[scancode];
                        }
                        
                        if (c) {
                            input_push_key((uint32_t)c, true);
                        }
                    }
                }
            }
        }
        loops++;
    }
}

extern "C" void keyboard_init()
{
    serial("[PS/2] Initializing keyboard...\n");

    /* 1. Disable Devices */
    kbd_wait_write();
    outb(PS2_CMD, 0xAD); // Disable Keyboard
    kbd_wait_write();
    outb(PS2_CMD, 0xA7); // Disable Mouse

    /* 2. Flush Output Buffer */
    while (inb(PS2_STATUS) & STATUS_OUTPUT_FULL) inb(PS2_DATA);

    /* 3. Set Configuration Byte */
    kbd_wait_write();
    outb(PS2_CMD, 0x20); // Read Config
    kbd_wait_read();
    uint8_t config = inb(PS2_DATA);

    config |= 0x01;  // Enable IRQ1 (Keyboard)
    config |= 0x02;  // Enable IRQ12 (Mouse) - Critical for mouse interrupts!
    config |= 0x40;  // Enable Translation (Set 2 -> Set 1)
    
    kbd_wait_write();
    outb(PS2_CMD, 0x60); // Write Config
    kbd_wait_write();
    outb(PS2_DATA, config);

    /* 4. Enable Keyboard */
    kbd_wait_write();
    outb(PS2_CMD, 0xAE);

    /* 5. Reset Keyboard */
    kbd_wait_write();
    outb(PS2_DATA, 0xFF);
    kbd_wait_read();
    inb(PS2_DATA); // ACK
    kbd_wait_read();
    inb(PS2_DATA); // BAT

    /* 6. Enable Scanning (Explicit) - Fixes dead keyboard after reboot */
    kbd_wait_write();
    outb(PS2_DATA, 0xF4);
    kbd_wait_read();
    inb(PS2_DATA); // ACK

    /* 7. Install Handler */
    irq_install_handler(1, keyboard_handler);
    
    /* 8. Unmask IRQ1 in PIC (Master) */
    uint8_t mask = inb(0x21);
    outb(0x21, mask & 0xFD);

    serial("[PS/2] Keyboard initialized.\n");
}

/* --- Watchdog Implementation --- */
static int watchdog_stuck_count = 0;

void ps2_controller_watchdog(void) {
    uint8_t status = inb(PS2_STATUS);
    
    /* Verificăm dacă există date în buffer (Bit 0 = 1) */
    if (status & STATUS_OUTPUT_FULL) {
        watchdog_stuck_count++;
        
        /* Dacă datele stau necitite prea mult timp (aprox 2-3 secunde la 100Hz),
           înseamnă că întreruperile nu s-au declanșat sau controllerul e blocat. */
        if (watchdog_stuck_count > 200) {
            serial("[PS/2] Watchdog: Stuck data detected (Status=0x%x). Flushing...\n", status);
            
            /* Citim forțat pentru a debloca linia */
            uint8_t garbage = inb(PS2_DATA);
            (void)garbage;
            
            /* Dacă problema persistă (aprox 5 secunde), dăm reload complet */
            if (watchdog_stuck_count > 500) {
                serial("[PS/2] Watchdog: Devices unresponsive. Reloading drivers...\n");
                keyboard_init();
                mouse_init();
                watchdog_stuck_count = 0;
            }
        }
    } else {
        /* Buffer gol, totul e OK */
        watchdog_stuck_count = 0;
    }
}
