#pragma once
#include "../ui/wm/window.h"
#include "../input/input.h"

#ifdef __cplusplus
extern "C" {
#endif

void minesweeper_app_create(void);
window_t* minesweeper_app_get_window(void);
void minesweeper_app_handle_event(input_event_t* ev);

#ifdef __cplusplus
}
#endif