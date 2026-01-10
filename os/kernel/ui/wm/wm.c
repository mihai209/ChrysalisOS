#include "wm.h"
#include "../../video/compositor.h"
#include "../../mem/kmalloc.h"
#include <stddef.h>
#include "../../terminal.h"

/* Import serial logging */
extern void serial(const char *fmt, ...);

#define MAX_WINDOWS 32

static window_t* windows_list = NULL;
static window_t* focused_window = NULL;
static wm_layout_t* current_layout = &wm_layout_floating;
static uint32_t next_win_id = 1;
static bool wm_dirty = true;

/* Array for layout processing */
static window_t* win_array[MAX_WINDOWS];
static size_t win_count = 0;

void wm_init(void) {
    windows_list = NULL;
    focused_window = NULL;
    current_layout = &wm_layout_floating;
    win_count = 0;
    wm_dirty = true;
    serial("[WM] Initialized\n");
}

void wm_mark_dirty(void) {
    wm_dirty = true;
}

bool wm_is_dirty(void) {
    return wm_dirty;
}

static void rebuild_win_array(void) {
    win_count = 0;
    window_t* cur = windows_list;
    while (cur && win_count < MAX_WINDOWS) {
        win_array[win_count++] = cur;
        cur = cur->next;
    }
}

window_t* wm_create_window(surface_t* surface, int x, int y) {
    if (win_count >= MAX_WINDOWS) {
        serial("[WM] Error: Max windows reached\n");
        return NULL;
    }

    window_t* win = window_create(surface, x, y);
    if (!win) return NULL;

    win->id = next_win_id++;
    
    /* Add to list */
    win->next = windows_list;
    windows_list = win;
    
    rebuild_win_array();

    serial("[WM] Window created id=%u\n", win->id);

    wm_hooks_t* hooks = wm_get_hooks();
    if (hooks && hooks->on_window_create) {
        hooks->on_window_create(win);
    }

    wm_focus_window(win);
    wm_dirty = true;
    return win;
}

void wm_destroy_window(window_t* win) {
    if (!win) return;

    /* Remove from list */
    if (windows_list == win) {
        windows_list = win->next;
    } else {
        window_t* cur = windows_list;
        while (cur && cur->next != win) {
            cur = cur->next;
        }
        if (cur) {
            cur->next = win->next;
        }
    }

    rebuild_win_array();

    if (focused_window == win) {
        focused_window = windows_list; /* Focus first available or NULL */
        serial("[WM] Focus changed (auto)\n");
    }

    wm_hooks_t* hooks = wm_get_hooks();
    if (hooks && hooks->on_window_destroy) {
        hooks->on_window_destroy(win);
    }

    serial("[WM] Window destroyed id=%u\n", win->id);
    window_destroy(win);
    wm_dirty = true;
}

void wm_focus_window(window_t* win) {
    if (focused_window == win) return;

    window_t* old = focused_window;
    focused_window = win;

    serial("[WM] Focus changed\n");

    wm_hooks_t* hooks = wm_get_hooks();
    if (hooks && hooks->on_focus_change) {
        hooks->on_focus_change(old, win);
    }
    wm_dirty = true;
}

window_t* wm_get_focused(void) {
    return focused_window;
}

void wm_set_layout(wm_layout_t* layout) {
    if (layout) {
        current_layout = layout;
        serial("[WM] Layout set to %s\n", layout->name);
    }
    wm_dirty = true;
}

window_t* wm_find_window_at(int x, int y) {
    /* Iterate from Newest (Top) to Oldest (Bottom) */
    for (size_t i = 0; i < win_count; i++) {
        window_t* w = win_array[i];
        if (!w || !w->surface) continue;
        
        if (x >= w->x && x < w->x + (int)w->surface->width &&
            y >= w->y && y < w->y + (int)w->surface->height) {
            return w;
        }
    }
    return NULL;
}

void wm_render(void) {
    if (!wm_dirty && !terminal_is_dirty()) return;

    /* 1. Apply Layout */
    if (current_layout && current_layout->apply) {
        current_layout->apply(win_array, win_count);
    }

    /* 2. Prepare Surface List for Compositor (Bottom to Top) */
    surface_t* render_list[MAX_WINDOWS];
    int render_count = 0;

    /* Iterate backwards (Oldest/Bottom -> Newest/Top) for Painter's Algorithm */
    for (int i = (int)win_count - 1; i >= 0; i--) {
        window_t* w = win_array[i];
        if (w->surface) {
            w->surface->x = w->x;
            w->surface->y = w->y;
            render_list[render_count++] = w->surface;
        }
    }

    /* 3. Hooks */
    wm_hooks_t* hooks = wm_get_hooks();
    if (hooks && hooks->on_frame) {
        hooks->on_frame();
    }

    /* 4. Render */
    serial("[WM] Rendering\n");
    compositor_render_surfaces(render_list, render_count);
    
    wm_dirty = false;
    terminal_clear_dirty();
}