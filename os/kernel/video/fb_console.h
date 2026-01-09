#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char c;
    uint32_t fg;
    uint32_t bg;
} console_cell_t;

void fb_cons_init(void);
void fb_cons_putc(char c);
void fb_cons_puts(const char* s);
void fb_cons_clear(void);
void fb_cons_scroll(int lines);

/* VT Integration API */
void fb_cons_get_dims(uint32_t* cols, uint32_t* rows);

/* Swaps the active buffer and cursor pointers used by the renderer */
void fb_cons_set_state(console_cell_t* new_buf, uint32_t* new_cx, uint32_t* new_cy);
void fb_cons_redraw(void);

#ifdef __cplusplus
}
#endif