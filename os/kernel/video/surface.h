#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t width;
    uint32_t height;
    int32_t x;
    int32_t y;
    bool visible;
    uint32_t* pixels; /* Buffer in RAM (ARGB/RGB) */
} surface_t;

surface_t* surface_create(uint32_t width, uint32_t height);
void surface_destroy(surface_t* surface);
void surface_clear(surface_t* surface, uint32_t color);
void surface_put_pixel(surface_t* surface, uint32_t x, uint32_t y, uint32_t color);

#ifdef __cplusplus
}
#endif