#include "compositor.h"
#include "framebuffer.h" /* fb_putpixel, fb_clear, fb_get_info */

/* Import serial logging */
extern void serial(const char *fmt, ...);

/* Simple static array for surfaces */
#define MAX_SURFACES 32
static surface_t* surfaces[MAX_SURFACES];
static int surface_count = 0;

void compositor_init(void) {
    for (int i = 0; i < MAX_SURFACES; i++) {
        surfaces[i] = 0;
    }
    surface_count = 0;
    serial("[COMPOSITOR] Initialized.\n");
}

void compositor_add_surface(surface_t* surface) {
    if (!surface) return;
    
    /* Check if already added */
    for (int i = 0; i < MAX_SURFACES; i++) {
        if (surfaces[i] == surface) return;
    }

    /* Find empty slot */
    for (int i = 0; i < MAX_SURFACES; i++) {
        if (surfaces[i] == 0) {
            surfaces[i] = surface;
            surface_count++;
            serial("[COMPOSITOR] Added surface 0x%x at index %d\n", surface, i);
            return;
        }
    }
    serial("[COMPOSITOR] Error: Max surfaces reached.\n");
}

void compositor_remove_surface(surface_t* surface) {
    if (!surface) return;
    
    for (int i = 0; i < MAX_SURFACES; i++) {
        if (surfaces[i] == surface) {
            surfaces[i] = 0;
            surface_count--;
            serial("[COMPOSITOR] Removed surface 0x%x from index %d\n", surface, i);
            return;
        }
    }
}

void compositor_render(void) {
    serial("[COMPOSITOR] Rendering frame...\n");

    /* 1. Clear Framebuffer (Background) */
    /* Using a default background color (Black) */
    fb_clear(0x000000);

    uint32_t fb_w = 0, fb_h = 0;
    fb_get_info(&fb_w, &fb_h, 0, 0, 0);

    if (fb_w == 0 || fb_h == 0) {
        serial("[COMPOSITOR] Error: Framebuffer not ready (w=%d, h=%d)\n", fb_w, fb_h);
        return;
    }

    /* 2. Iterate Surfaces */
    for (int i = 0; i < MAX_SURFACES; i++) {
        surface_t* s = surfaces[i];
        if (!s || !s->visible) continue;

        /* 3. Blit Surface */
        /* Simple clipping logic */
        int32_t start_x = (s->x < 0) ? -s->x : 0;
        int32_t start_y = (s->y < 0) ? -s->y : 0;
        
        int32_t end_x = s->width;
        int32_t end_y = s->height;

        /* Clip against screen right/bottom */
        if (s->x + end_x > (int32_t)fb_w) end_x = (int32_t)fb_w - s->x;
        if (s->y + end_y > (int32_t)fb_h) end_y = (int32_t)fb_h - s->y;

        /* Draw visible pixels */
        for (int32_t y = start_y; y < end_y; y++) {
            for (int32_t x = start_x; x < end_x; x++) {
                /* Get pixel from surface buffer */
                uint32_t color = s->pixels[y * s->width + x];
                
                /* Draw to framebuffer */
                /* Coordinates on screen */
                uint32_t screen_x = s->x + x;
                uint32_t screen_y = s->y + y;
                
                /* Note: fb_putpixel handles bounds checking internally usually, 
                   but our clipping above ensures we are mostly safe. */
                fb_putpixel(screen_x, screen_y, color);
            }
        }
    }
}