// panic.cpp - minimal "BSOD-like" crash screen for Chrysalis OS
#include "panic.h"
#include <stddef.h>
#include <stdint.h>

/* VGA text-mode */
#define VGA_MEM ((volatile uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25

/* attribute: background blue (1) << 4, foreground white (15) */
static const uint8_t ATTR_BLUE_BG_WHITE_FG = (1 << 4) | 15;

static void clear_screen_with_attr(uint8_t attr) {
    volatile uint16_t *v = VGA_MEM;
    uint16_t cell = ((uint16_t)attr << 8) | (uint8_t)' ';
    for (size_t i = 0; i < VGA_W * VGA_H; ++i) v[i] = cell;
}

static void vga_write_at(const char *s, int row, int col, uint8_t attr) {
    volatile uint16_t *v = VGA_MEM;
    int pos = row * VGA_W + col;
    while (*s && pos < VGA_W * VGA_H) {
        if (*s == '\n') {
            int current_row = pos / VGA_W;
            ++current_row;
            pos = current_row * VGA_W;
            ++s;
            continue;
        }
        v[pos++] = ((uint16_t)attr << 8) | (uint8_t)(*s++);
    }
}

void panic(const char *message) {
    /* disable interrupts and avoid touching other kernel subsystems */
    asm volatile("cli");

    /* paint screen blue and write header + message */
    clear_screen_with_attr(ATTR_BLUE_BG_WHITE_FG);

    vga_write_at("CHRYSALIS - KERNEL PANIC", 1, 0, ATTR_BLUE_BG_WHITE_FG);
    vga_write_at("A fatal error occurred. The system has been halted.", 3, 0, ATTR_BLUE_BG_WHITE_FG);

    if (message && *message) {
        vga_write_at("DETAILS:", 5, 0, ATTR_BLUE_BG_WHITE_FG);
        vga_write_at(message, 6, 0, ATTR_BLUE_BG_WHITE_FG);
    } else {
        vga_write_at("(no message provided)", 6, 0, ATTR_BLUE_BG_WHITE_FG);
    }

    vga_write_at("", 8, 0, ATTR_BLUE_BG_WHITE_FG);
    vga_write_at("If you are debugging in QEMU, you can restart the VM.", 10, 0, ATTR_BLUE_BG_WHITE_FG);
    vga_write_at("Pressing any key won't resume execution. System halted.", 12, 0, ATTR_BLUE_BG_WHITE_FG);

    /* halt forever */
    for (;;) {
        asm volatile("hlt");
    }
}
