#include "widgets.h"
#include "../draw.h"
#include "../theme.h"

static void panel_draw(fly_widget_t* w, surface_t* surf, int x, int y) {
    /* Fill background */
    fly_draw_rect_fill(surf, x, y, w->w, w->h, w->bg_color);
    
    /* Draw border using theme color (Low/Dark for outline) */
    fly_theme_t* th = theme_get();
    fly_draw_rect_outline(surf, x, y, w->w, w->h, th->color_lo_2);
}

fly_widget_t* fly_panel_create(int w, int h) {
    fly_widget_t* widget = fly_widget_create();
    widget->w = w;
    widget->h = h;
    widget->on_draw = panel_draw;
    
    /* Set default background from theme */
    fly_theme_t* th = theme_get();
    widget->bg_color = th->win_bg;
    
    return widget;
}