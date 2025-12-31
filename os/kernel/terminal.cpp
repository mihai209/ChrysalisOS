#include "terminal.h"

static uint16_t* vga = (uint16_t*)0xB8000;
static int row = 0, col = 0;

extern "C" void terminal_putchar(char c) {
    if (c == '\n') {
        row++;
        col = 0;
        return;
    }

    vga[row * 80 + col] = (uint16_t)c | 0x0F00;
    col++;

    if (col >= 80) {
        col = 0;
        row++;
    }
}
