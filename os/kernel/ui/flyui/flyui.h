#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../../video/surface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event Types */
typedef enum {
    FLY_EVENT_NONE = 0,
    FLY_EVENT_MOUSE_MOVE,
    FLY_EVENT_MOUSE_DOWN,
    FLY_EVENT_MOUSE_UP,
    FLY_EVENT_KEY_DOWN,
    FLY_EVENT_KEY_UP
} fly_event_type_t;

/* Event Structure */
typedef struct {
    fly_event_type_t type;
    int mx, my;      /* Mouse coordinates (relative to window/widget depending on context) */
    int keycode;     /* Keyboard key */
} fly_event_t;

/* Forward declaration */
struct fly_widget;

/* Widget Callbacks */
typedef void (*fly_draw_func_t)(struct fly_widget* w, surface_t* surf, int x, int y);
typedef bool (*fly_event_func_t)(struct fly_widget* w, fly_event_t* e);

/* Widget Structure */
typedef struct fly_widget {
    int x, y, w, h;
    uint32_t bg_color;
    uint32_t fg_color;
    
    struct fly_widget* parent;
    struct fly_widget* first_child;
    struct fly_widget* next_sibling;
    
    void* internal_data; /* Widget specific data (e.g. text) */
    
    fly_draw_func_t on_draw;
    fly_event_func_t on_event;
} fly_widget_t;

/* Context Structure */
typedef struct {
    surface_t* surface;
    fly_widget_t* root;
} flyui_context_t;

/* Core API */
flyui_context_t* flyui_init(surface_t* surface);
void flyui_set_root(flyui_context_t* ctx, fly_widget_t* root);
void flyui_render(flyui_context_t* ctx);
void flyui_dispatch_event(flyui_context_t* ctx, fly_event_t* event);
fly_widget_t* fly_widget_create(void);
void fly_widget_add(fly_widget_t* parent, fly_widget_t* child);

#ifdef __cplusplus
}
#endif