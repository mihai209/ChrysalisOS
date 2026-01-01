#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void shortcuts_handle_scancode(uint8_t scancode);

extern volatile bool shortcut_ctrl_c;

#ifdef __cplusplus
}
#endif
