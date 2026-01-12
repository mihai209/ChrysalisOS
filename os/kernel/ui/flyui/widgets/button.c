#include "widgets.h"
#include "../draw.h"
#include "../theme.h"
#include "../../../mem/kmalloc.h"
#include "../../../string.h"

extern void serial(const char *fmt, ...);

typedef struct {
    char* text;
    bool pressed;
} button_data_t;

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

static void button_draw(fly_widget_t* w, surface_t* surf, int x, int y) {
    button_data_t* d = (button_data_t*)w->internal_data;
    uint32_t bg = w->bg_color;
    uint32_t fg = w->fg_color;
    fly_theme_t* th = theme_get();
    
    /* 3D Bevel Logic */
    uint32_t c_tl = th->color_hi_1;
    uint32_t c_br = th->color_lo_2;
    uint32_t c_br_inner = th->color_lo_1;

    if (d && d->pressed) { 
        /* Invert bevels for pressed state */
        c_tl = th->color_lo_2;
        c_br = th->color_hi_1;
        c_br_inner = th->color_lo_1;
        /* Shift content slightly */
        x += 1; y += 1;
    }

    fly_draw_rect_fill(surf, x, y, w->w, w->h, bg);
    
    /* Outer Border */
    fly_draw_rect_outline(surf, x, y, w->w, w->h, 0xFF000000);
    
    /* Bevels */
    /* Top & Left (Highlight) */
    fly_draw_line(surf, x+1, y+1, x+w->w-2, y+1, c_tl);
    fly_draw_line(surf, x+1, y+1, x+1, y+w->h-2, c_tl);
    
    /* Bottom & Right (Shadow) */
    fly_draw_line(surf, x+1, y+w->h-2, x+w->w-2, y+w->h-2, c_br);
    fly_draw_line(surf, x+w->w-2, y+1, x+w->w-2, y+w->h-2, c_br);
    
    /* Inner Shadow (Bottom/Right only for classic feel) */
    if (!d || !d->pressed) {
        fly_draw_line(surf, x+2, y+w->h-3, x+w->w-3, y+w->h-3, c_br_inner);
        fly_draw_line(surf, x+w->w-3, y+2, x+w->w-3, y+w->h-3, c_br_inner);
    }
    
    /* Draw text centered */
    if (d && d->text) {
        int text_w = strlen(d->text) * 8;
        int tx = x + (w->w - text_w) / 2;
        int ty = y + (w->h - 16) / 2;
        fly_draw_text(surf, tx, ty, d->text, fg);
    }
}

static bool button_event(fly_widget_t* w, fly_event_t* e) {
    button_data_t* d = (button_data_t*)w->internal_data;
    if (!d) return false;

    if (e->type == FLY_EVENT_MOUSE_DOWN) {
        d->pressed = true;
        serial("[FLYUI] button clicked: %s\n", d ? d->text : "???");
        return true;
    } else if (e->type == FLY_EVENT_MOUSE_UP) {
        d->pressed = false;
        return true;
    }
    return false;
}

fly_widget_t* fly_button_create(const char* text) {
    fly_widget_t* w = fly_widget_create();
    button_data_t* d = (button_data_t*)kmalloc(sizeof(button_data_t));
    if (d) {
        d->text = (char*)kmalloc(strlen(text) + 1);
        strcpy(d->text, text);
        d->pressed = false;
        w->internal_data = d;
    }
    w->on_draw = button_draw;
    w->on_event = button_event;
    w->bg_color = 0xFFC0C0C0; /* Classic Gray */
    w->fg_color = 0xFF000000;
    return w;
}
