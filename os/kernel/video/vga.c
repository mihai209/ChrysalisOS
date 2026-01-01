#include "vga.h"
#include <stdint.h>

/* Default VGA 320x200 8bpp memory (mode 13h) */
#define VGA_DEFAULT_ADDR ((uint8_t*)0xA0000)
#define VGA_DEFAULT_WIDTH 320
#define VGA_DEFAULT_HEIGHT 200
#define VGA_DEFAULT_PITCH 320
#define VGA_DEFAULT_BPP 8

/* current framebuffer descriptor */
static uint8_t* fb_addr = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;

void vga_set_framebuffer(void* addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp)
{
    fb_addr = (uint8_t*)addr;
    fb_width = width;
    fb_height = height;
    fb_pitch = pitch;
    fb_bpp = bpp;
}

void vga_init(void)
{
    /* If a framebuffer was already provided, keep it.
       Otherwise fall back to the legacy VGA A0000 320x200x8 mode buffer.
       IMPORTANT: we DO NOT call BIOS interrupts here (no int 0x10). */
    if (!fb_addr) {
        fb_addr = VGA_DEFAULT_ADDR;
        fb_width = VGA_DEFAULT_WIDTH;
        fb_height = VGA_DEFAULT_HEIGHT;
        fb_pitch = VGA_DEFAULT_PITCH;
        fb_bpp = VGA_DEFAULT_BPP;
    }

    /* Nothing else to do at init time for our simple driver.
       Real drivers would set palette, modes, etc. */
}

/* internal helpers for pixel write depending on bpp */
static inline void putpixel_8(int x, int y, uint8_t color)
{
    uint8_t* dest = fb_addr + (y * fb_pitch) + x;
    *dest = color;
}

static inline void putpixel_24(int x, int y, uint8_t color)
{
    uint8_t* dest = fb_addr + (y * fb_pitch) + x * 3;
    /* map 0..255 color to grey RGB */
    dest[0] = color; /* B */
    dest[1] = color; /* G */
    dest[2] = color; /* R */
}

static inline void putpixel_32(int x, int y, uint8_t color)
{
    uint32_t* dest = (uint32_t*)(fb_addr + (y * fb_pitch) + x * 4);
    uint32_t v = (0x00 << 24) | (color << 16) | (color << 8) | color; /* 0x00RRGGBB */
    *dest = v;
}

void vga_putpixel(int x, int y, uint8_t color)
{
    if (!fb_addr) return;
    if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height) return;

    if (fb_bpp == 8) {
        putpixel_8(x, y, color);
    } else if (fb_bpp == 24) {
        putpixel_24(x, y, color);
    } else if (fb_bpp == 32) {
        putpixel_32(x, y, color);
    } else {
        /* unknown bpp - fallback to 8bpp write */
        putpixel_8(x, y, color);
    }
}

void vga_clear(uint8_t color)
{
    if (!fb_addr) return;

    if (fb_bpp == 8) {
        for (uint32_t y = 0; y < fb_height; y++) {
            uint8_t* row = fb_addr + y * fb_pitch;
            for (uint32_t x = 0; x < fb_width; x++)
                row[x] = color;
        }
    } else if (fb_bpp == 24) {
        for (uint32_t y = 0; y < fb_height; y++) {
            uint8_t* row = fb_addr + y * fb_pitch;
            for (uint32_t x = 0; x < fb_width; x++) {
                row[x*3 + 0] = color;
                row[x*3 + 1] = color;
                row[x*3 + 2] = color;
            }
        }
    } else if (fb_bpp == 32) {
        for (uint32_t y = 0; y < fb_height; y++) {
            uint32_t* row = (uint32_t*)(fb_addr + y * fb_pitch);
            uint32_t v = (0x00 << 24) | (color << 16) | (color << 8) | color;
            for (uint32_t x = 0; x < fb_width; x++)
                row[x] = v;
        }
    } else {
        /* fallback: wipe first fb_pitch * fb_height bytes */
        for (uint32_t y = 0; y < fb_height; y++) {
            uint8_t* row = fb_addr + y * fb_pitch;
            for (uint32_t i = 0; i < fb_pitch; i++)
                row[i] = 0;
        }
    }
}

void vga_draw_rect(int x, int y, int w, int h, uint8_t color)
{
    if (!fb_addr) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;

    for (int yy = y; yy < y + h && yy < (int)fb_height; yy++) {
        for (int xx = x; xx < x + w && xx < (int)fb_width; xx++) {
            vga_putpixel(xx, yy, color);
        }
    }
}
