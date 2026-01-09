#pragma once
#include "surface.h"

#ifdef __cplusplus
extern "C" {
#endif

void compositor_init(void);
void compositor_add_surface(surface_t* surface);
void compositor_remove_surface(surface_t* surface);
void compositor_render(void);

#ifdef __cplusplus
}
#endif