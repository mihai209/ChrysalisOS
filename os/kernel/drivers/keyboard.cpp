// kernel/drivers/keyboard.cpp
#include <stdint.h>
#include <stdbool.h>

#include "keyboard.h"

#include "../interrupts/irq.h"
#include "../shortcuts/shortcuts.h"
#include "../input/keyboard_buffer.h"

/* include keymap */
#include "../keyboard/keymap.h"
#include "../keyboard/keymaps.h"

/* include event queue */
#include "../events/event.h"
#include "../events/event_queue.h"

/* state flags */
static int ctrl_pressed = 0;
static int shift_pressed = 0;
static int altgr_pressed = 0;   // <- adăugat

/* trimite EOI la PIC */
static inline void pic_send_eoi(void) {
    uint8_t val = 0x20;
    uint16_t port = 0x20;
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

extern "C" void keyboard_handler(registers_t* regs)
{
    (void)regs;

    uint8_t scancode;
    asm volatile("inb %1, %0" : "=a"(scancode) : "Nd"(0x60));

    bool handled = false;

    /* --- KEY RELEASES --- */
    if (scancode & 0x80) {
        uint8_t sc = scancode & 0x7F;

        /* Shift release */
        if (sc == 0x2A || sc == 0x36) {
            shift_pressed = 0;
            handled = true;
        }

        /* Ctrl release (0x1D + 0x80 = 0x9D) */
        if (sc == 0x1D) {
            ctrl_pressed = 0;
            handled = true;
        }

        /* AltGr (Right Alt) release - simplificat */
        if (sc == 0x38) {
            altgr_pressed = 0;
            handled = true;
        }

        /* push event release */
        event_t ev;
        ev.type = EVENT_KEY;
        ev.key.scancode = sc;
        ev.key.ascii = 0; /* no ascii on release */
        ev.key.pressed = 0;
        (void)event_push(&ev); /* ignore failure */

        /* ignore other releases */
        if (!handled)
            handled = true;

    } else {
        /* --- KEY PRESSES --- */

        /* Shift press */
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
            handled = true;
        }
        /* Ctrl press */
        else if (scancode == 0x1D) {
            ctrl_pressed = 1;
            handled = true;
        }
        /* AltGr (Right Alt) press - simplificat */
        else if (scancode == 0x38) {
            altgr_pressed = 1;
            handled = true;
        }
        else {
            /* translate scancode -> char using current keymap + shift + altgr flags */
            char c = keymap_translate(scancode, shift_pressed, altgr_pressed);

            /* push event (press) into queue */
            event_t ev;
            ev.type = EVENT_KEY;
            ev.key.scancode = scancode;
            ev.key.ascii = c;
            ev.key.pressed = 1;
            (void)event_push(&ev); /* best-effort; drop if full */

            if (c) {
                /* notify shortcuts about the produced character (ex: 'l') */
                shortcuts_notify_char(c);
            }

            /* Ctrl + key shortcuts (if Ctrl is held) */
            if (ctrl_pressed) {
                if (shortcuts_handle_ctrl(scancode)) {
                    handled = true;
                }
            }

            /* normal input: keep kbd_push for compatibility (shell) */
            if (!handled) {
                if (c) {
                    kbd_push(c);
                    handled = true;
                }
            }
        }
    }

    /* manual EOI pentru PIC (dacă wrapper-ul tău deja trimite EOI, poți elimina) */
    pic_send_eoi();

    (void)handled;
    return;
}

extern "C" void keyboard_init()
{
    /* setăm keymap-ul implicit (US) */
    keymap_set(keymap_us_ptr);

    irq_install_handler(1, keyboard_handler);
}
