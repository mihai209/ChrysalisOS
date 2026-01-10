#pragma once
#include "surface.h"

#ifdef __cplusplus
extern "C" {
#endif

void compositor_init(void);
/* Render a specific list of surfaces (allows WM to control Z-order) */
void compositor_render_surfaces(surface_t** surfaces, int count);

#ifdef __cplusplus
}
#endif