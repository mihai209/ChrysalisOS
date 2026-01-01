#include "keymap.h"

const keymap_t* current_keymap = 0;

void keymap_set(const keymap_t* map) {
    current_keymap = map;
}

char keymap_translate(uint8_t scancode, int shift) {
    if (!current_keymap)
        return 0;

    if (scancode >= 128)
        return 0;

    if (shift)
        return current_keymap->shift[scancode];

    return current_keymap->normal[scancode];
}
