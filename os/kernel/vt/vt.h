#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Virtual Terminals Subsystem */

#define VT_COUNT 6

void vt_init(void);
void vt_putc(char c);
void vt_write(const char* s);
void vt_switch(int id);
int  vt_active(void);
void vt_clear(int id);

/* Info / Metadata API */
const char* vt_get_role(int id);
void vt_set_role(int id, const char* role);

/* Helper to route input to the active VT's shell */
void vt_shell_handle_char(char c);

#ifdef __cplusplus
}
#endif