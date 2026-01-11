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

static void button_draw(fly_widget_t* w, surface_t* surf, int x, int y) {
    button_data_t* d = (button_data_t*)w->internal_data;
    uint32_t bg = w->bg_color;
    uint32_t fg = w->fg_color;
    
    if (d && d->pressed) { bg = 0xFF000000; fg = 0xFFFFFFFF; } /* Invert on press */

    /* Classic Style: Gray background */
    fly_draw_rect_fill(surf, x, y, w->w, w->h, bg);
    fly_draw_rect_outline(surf, x, y, w->w, w->h, 0xFF000000);
    /* Double border for "3D" effect */
    fly_draw_rect_outline(surf, x + 2, y + 2, w->w - 4, w->h - 4, FLY_COLOR_BORDER);
    
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
