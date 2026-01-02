#pragma once
#include <stdint.h>

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEY,
    EVENT_TIMER,
    EVENT_MOUSE
} event_type_t;

typedef struct {
    event_type_t type;

    union {
        struct {
            uint8_t scancode;
            char ascii;
            uint8_t pressed;
        } key;

        struct {
            uint32_t ticks;
        } timer;

        struct {
            int dx, dy;
            uint8_t buttons;
        } mouse;
    };
} event_t;
