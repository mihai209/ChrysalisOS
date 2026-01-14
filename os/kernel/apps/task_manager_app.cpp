#include "task_manager_app.h"
#include "../ui/wm/wm.h"
#include "../ui/flyui/draw.h"
#include "../ui/flyui/theme.h"
#include "app_manager.h"
#include "../string.h"

static window_t* task_win = NULL;

static void fly_draw_line(surface_t* surf, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? -(y1 - y0) : -(y0 - y1);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int e2;

    for (;;) {
        if (x0 >= 0 && x0 < (int)surf->width && y0 >= 0 && y0 < (int)surf->height) {
            surf->pixels[y0 * surf->width + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Helper pentru desenarea butoanelor tip "Classic"
static void draw_classic_button(surface_t* s, int x, int y, int w, int h, const char* text) {
    fly_theme_t* th = theme_get();
    fly_draw_rect_fill(s, x, y, w, h, th->win_bg);
    
    // Border 3D effect
    fly_draw_line(s, x, y, x + w, y, th->color_hi_1);         // Top
    fly_draw_line(s, x, y, x, y + h, th->color_hi_1);         // Left
    fly_draw_line(s, x + w - 1, y, x + w - 1, y + h, th->color_lo_2); // Right
    fly_draw_line(s, x, y + h - 1, x + w, y + h - 1, th->color_lo_2); // Bottom
    
    fly_draw_text(s, x + (w/4), y + (h/4), text, th->color_text);
}

static void draw_stats(surface_t* s) {
    fly_theme_t* th = theme_get();
    int w = s->width;
    
    // Header
    fly_draw_rect_fill(s, 0, 0, w, 24, th->win_title_active_bg);
    fly_draw_text(s, 5, 4, "Task Manager", th->win_title_active_fg);

    // Close Button (Folosind codul tău de linie)
    int bx = w - 20;
    draw_classic_button(s, bx, 4, 16, 16, "X");

    // Separator sub titlu
    fly_draw_rect_fill(s, 0, 24, w, 1, th->color_lo_1);

    // List Apps
    fly_draw_text(s, 10, 30, "Running Tasks:", th->color_text);
    
    app_info_t* app = app_get_list();
    int y = 50;
    while (app) {
        // Checkbox outline
        fly_draw_rect_outline(s, 10, y, 12, 12, th->color_text);
        if (app->selected) {
            fly_draw_rect_fill(s, 12, y + 2, 8, 8, th->color_text);
        }
        
        fly_draw_text(s, 30, y, app->name, th->color_text);
        
        y += 20;
        app = app->next;
        if (y > (int)s->height - 60) break;
    }
    
    // "End Task" Button
    draw_classic_button(s, 150, s->height - 40, 80, 25, "End Task");
}

void task_manager_app_create(void) {
    if (task_win) return;
    
    surface_t* s = surface_create(250, 300);
    fly_theme_t* th = theme_get();
    surface_clear(s, th->win_bg);

    draw_stats(s);

    task_win = wm_create_window(s, 200, 200);
    app_register("Task Manager", task_win);
}

bool task_manager_app_handle_event(input_event_t* ev) {
    if (!task_win) return false;

    if (ev->type == INPUT_MOUSE_CLICK && ev->pressed) {
        int lx = ev->mouse_x - task_win->x;
        int ly = ev->mouse_y - task_win->y;
        int surf_w = task_win->surface->width;

        // Close logic
        if (lx >= surf_w - 20 && ly < 24) {
            wm_destroy_window(task_win);
            app_unregister(task_win);
            task_win = NULL;
            wm_mark_dirty();
            return true;
        }

        // Kill logic (simplificată)
        if (lx >= 150 && lx <= 230 && ly >= 260) {
             app_info_t* app = app_get_list();
             while(app) {
                 if(app->selected && app->window != task_win) {
                     wm_destroy_window(app->window);
                     app_unregister(app->window);
                     app = app_get_list(); // Refresh list
                     continue;
                 }
                 app = app->next;
             }
             // Redraw stats
             surface_clear(task_win->surface, theme_get()->win_bg);
             draw_stats(task_win->surface);
             wm_mark_dirty();
             return true;
        }
    }
    return false;
}

void task_manager_app_update(void) {
    /* No-op for now */
}

window_t* task_manager_app_get_window(void) {
    return task_win;
}