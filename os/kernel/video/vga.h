#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize VGA subsystem.
   This function does NOT call BIOS interrupts.
   By default it will try to use 0xA0000 as an 8bpp framebuffer (mode 13h style).
   If you have a framebuffer from the bootloader, call vga_set_framebuffer() first. */
void vga_init(void);

/* Provide a framebuffer from the bootloader / multiboot:
   addr  - pointer to framebuffer memory
   width - pixel width
   height- pixel height
   pitch - bytes per scanline (may be >= width * bytes_per_pixel)
   bpp   - bits per pixel (supported: 8, 24, 32)
*/
void vga_set_framebuffer(void* addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp);

/* Basic drawing */
void vga_putpixel(int x, int y, uint8_t color);
void vga_clear(uint8_t color);

/* Optional helpers */
void vga_draw_rect(int x, int y, int w, int h, uint8_t color);

#ifdef __cplusplus
}
#endif
