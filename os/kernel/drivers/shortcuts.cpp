#include "shortcuts.h"

volatile bool shortcut_ctrl_c = false;

static bool ctrl_down = false;

void shortcuts_handle_scancode(uint8_t scancode)
{
    // Ctrl pressed
    if (scancode == 0x1D) {
        ctrl_down = true;
        return;
    }

    // Ctrl released
    if (scancode == 0x9D) {
        ctrl_down = false;
        return;
    }

    // C key
    if (ctrl_down && scancode == 0x2E) {
        shortcut_ctrl_c = true;
    }
}
