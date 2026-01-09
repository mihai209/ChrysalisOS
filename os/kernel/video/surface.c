#include "surface.h"
#include "../mem/kmalloc.h"

/* Import serial logging */
extern void serial(const char *fmt, ...);

surface_t* surface_create(uint32_t width, uint32_t height) {
    surface_t* surf = (surface_t*)kmalloc(sizeof(surface_t));
    if (!surf) return 0;

    surf->width = width;
    surf->height = height;
    surf->x = 0;
    surf->y = 0;
    surf->visible = true;
    
    /* Allocate pixel buffer (32bpp) */
    surf->pixels = (uint32_t*)kmalloc(width * height * sizeof(uint32_t));
    if (!surf->pixels) {
        kfree(surf);
        return 0;
    }

    /* Initialize to transparent/black */
    for (uint32_t i = 0; i < width * height; i++) {
        surf->pixels[i] = 0xFF000000; // Opaque black default
    }

    serial("[SURFACE] Created %dx%d surface at 0x%x\n", width, height, surf);
    return surf;
}

void surface_destroy(surface_t* surface) {
    if (!surface) return;
    if (surface->pixels) kfree(surface->pixels);
    kfree(surface);
    serial("[SURFACE] Destroyed surface 0x%x\n", surface);
}

void surface_clear(surface_t* surface, uint32_t color) {
    if (!surface || !surface->pixels) return;
    uint32_t count = surface->width * surface->height;
    for (uint32_t i = 0; i < count; i++) {
        surface->pixels[i] = color;
    }
}

void surface_put_pixel(surface_t* surface, uint32_t x, uint32_t y, uint32_t color) {
    if (!surface || !surface->pixels) return;
    if (x >= surface->width || y >= surface->height) return;
    
    /* Row-major: y * width + x */
    surface->pixels[y * surface->width + x] = color;
}