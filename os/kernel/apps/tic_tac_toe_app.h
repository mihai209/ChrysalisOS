#pragma once
#include "../input/input.h"
#include "../ui/wm/window.h"

#ifdef __cplusplus
extern "C" {
#endif

void tic_tac_toe_app_create(void);
bool tic_tac_toe_app_handle_event(input_event_t* ev);
window_t* tic_tac_toe_app_get_window(void);

#ifdef __cplusplus
}
#endif