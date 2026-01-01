#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct keymap {
    const char* name;

    // fără shift
    char normal[128];

    // cu shift
    char shift[128];
} keymap_t;

// keymap activ
extern const keymap_t* current_keymap;

// API
void keymap_set(const keymap_t* map);
char keymap_translate(uint8_t scancode, int shift);

#ifdef __cplusplus
}
#endif
