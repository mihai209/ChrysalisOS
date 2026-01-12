#include "widgets.h"
#include "../draw.h"
#include "../theme.h"
#include "../../../mem/kmalloc.h"
#include "../../../string.h"

typedef struct {
    char* text;
} label_data_t;

static void label_draw(fly_widget_t* w, surface_t* surf, int x, int y) {
    label_data_t* d = (label_data_t*)w->internal_data;
    if (d && d->text) {
        fly_draw_text(surf, x, y, d->text, w->fg_color);
    }
}

fly_widget_t* fly_label_create(const char* text) {
    fly_widget_t* w = fly_widget_create();
    if (!w) return NULL;
    
    label_data_t* d = (label_data_t*)kmalloc(sizeof(label_data_t));
    if (d) {
        int len = strlen(text);
        d->text = (char*)kmalloc(len + 1);
        strcpy(d->text, text);
        w->internal_data = d;
    }
    
    w->on_draw = label_draw;
    w->fg_color = theme_get()->color_text;
    w->w = strlen(text) * 8;
    w->h = 16;
    
    return w;
}
