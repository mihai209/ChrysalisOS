#pragma once
#include "../flyui.h"

#ifdef __cplusplus
extern "C" {
#endif

fly_widget_t* fly_label_create(const char* text);
fly_widget_t* fly_button_create(const char* text);

/* Panel helper */
fly_widget_t* fly_panel_create(int w, int h);

#ifdef __cplusplus
}
#endif
