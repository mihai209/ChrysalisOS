// kernel/terminal/terminal.h
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void terminal_putchar(char c);
void terminal_writestring(const char* s);
void terminal_clear();
void terminal_init();

/* Switch backend to Framebuffer Console */
void terminal_set_backend_fb(bool active);

/* ===== NEW: printf support ===== */
void terminal_printf(const char* fmt, ...);
void terminal_vprintf(const char* fmt, void* va); /* intern */

/* print value as hex (no 0x prefix), trimmed leading zeros */
void terminal_writehex(uint32_t v);

/* ===== Piping / Redirection Support ===== */

/* Redirect output to a memory buffer instead of screen */
void terminal_start_capture(char* buf, size_t max_len, size_t* out_len);
void terminal_end_capture(void);

/* Set input source for commands that read from stdin (pipe) */
void terminal_set_input(const char* buf, size_t len);
int terminal_read_char(void); /* Returns char or -1 if EOF/Empty */

#ifdef __cplusplus
}
#endif
