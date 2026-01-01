#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* existing APIs */
void vga_init(void);
void vga_set_framebuffer(void* addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp);
void vga_putpixel(int x, int y, uint8_t color);
void vga_clear(uint8_t color);
void vga_draw_rect(int x, int y, int w, int h, uint8_t color);

/* NEW: query framebuffer info */
uint32_t vga_get_width(void);
uint32_t vga_get_height(void);
uint8_t  vga_get_bpp(void);
int      vga_has_framebuffer(void); /* returns 1 if fb_addr != NULL, 0 otherwise */

#ifdef __cplusplus
}
#endif
