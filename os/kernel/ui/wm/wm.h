#pragma once
#include "window.h"
#include "wm_layout.h"
#include "wm_hooks.h"

#ifdef __cplusplus
extern "C" {
#endif

void wm_init(void);
window_t* wm_create_window(surface_t* surface, int x, int y);
void wm_destroy_window(window_t* win);

void wm_focus_window(window_t* win);
window_t* wm_get_focused(void);

void wm_render(void);
void wm_set_layout(wm_layout_t* layout);

/* Hit test: Find window at global coordinates */
window_t* wm_find_window_at(int x, int y);

void wm_mark_dirty(void);
bool wm_is_dirty(void);

#ifdef __cplusplus
}
#endif