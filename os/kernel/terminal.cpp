// kernel/terminal/terminal.cpp
#include "terminal.h"
#include <stdarg.h>
#include <stdint.h>

static uint16_t* vga = (uint16_t*)0xB8000;
static int row = 0;
static int col = 0;
static const uint8_t color = 0x0F;

static void scroll() {
    if (row < 25) return;

    for (int y = 1; y < 25; y++)
        for (int x = 0; x < 80; x++)
            vga[(y - 1) * 80 + x] = vga[y * 80 + x];

    for (int x = 0; x < 80; x++)
        vga[24 * 80 + x] = ' ' | (color << 8);

    row = 24;
}

extern "C" void terminal_clear() {
    for (int i = 0; i < 80 * 25; i++)
        vga[i] = ' ' | (color << 8);

    row = 0;
    col = 0;
}

extern "C" void terminal_putchar(char c) {
    if (c == '\n') {
        row++;
        col = 0;
        scroll();
        return;
    }

    if (c == '\b') {
        if (col > 0) col--;
        vga[row * 80 + col] = ' ' | (color << 8);
        return;
    }

    vga[row * 80 + col] = c | (color << 8);
    col++;

    if (col >= 80) {
        col = 0;
        row++;
        scroll();
    }
}

extern "C" void terminal_writestring(const char* s) {
    while (*s)
        terminal_putchar(*s++);
}

extern "C" void terminal_init() {
    terminal_clear();
}

/* ---------- PRINT HELPERS (expanded) ---------- */

/* print 32-bit hex WITHOUT "0x" prefix, fixed 8 digits */
static void print_hex32(uint32_t v)
{
    const char* hex = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        terminal_putchar(hex[(v >> (i * 4)) & 0xF]);
    }
}

/* print pointer as 0xXXXXXXXX (32-bit) */
static void print_ptr(uintptr_t p)
{
    terminal_writestring("0x");
    /* assume 32-bit guest; print low 32 bits */
    print_hex32((uint32_t)p);
}

/* print unsigned 32-bit decimal */
static void print_uint32(uint32_t v)
{
    char buf[12]; /* enough for 32-bit decimal */
    int i = 0;
    if (v == 0) {
        terminal_putchar('0');
        return;
    }
    while (v > 0 && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (v % 10u);
        v /= 10u;
    }
    while (i--)
        terminal_putchar(buf[i]);
}

/* print signed 32-bit decimal */
static void print_int32(int32_t v)
{
    if (v < 0) {
        terminal_putchar('-');
        /* careful with INT_MIN but that's fine for now */
        print_uint32((uint32_t)(- (int64_t)v));
    } else {
        print_uint32((uint32_t)v);
    }
}

/* ---------- vprintf implementation (keeps original void* va_ptr API) ---------- */
/* va_ptr expected to point to a va_list as in previous code */
extern "C" void terminal_vprintf(const char* fmt, void* va_ptr)
{
    va_list args;
    /* copy the caller's va_list */
    va_copy(args, *(va_list*)va_ptr);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            terminal_putchar(*fmt);
            continue;
        }

        fmt++;
        if (*fmt == 0) break;

        switch (*fmt) {
            case 'c': {
                char c = (char)va_arg(args, int);
                terminal_putchar(c);
                break;
            }

            case 's': {
                const char* s = va_arg(args, const char*);
                terminal_writestring(s ? s : "(null)");
                break;
            }

            case 'd': {
                int v = va_arg(args, int);
                print_int32((int32_t)v);
                break;
            }

            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                print_uint32((uint32_t)v);
                break;
            }

            case 'x':
            case 'X': {
                unsigned int v = va_arg(args, unsigned int);
                print_hex32((uint32_t)v);
                break;
            }

            case 'p': {
                void* p = va_arg(args, void*);
                print_ptr((uintptr_t)p);
                break;
            }

            case '%': {
                terminal_putchar('%');
                break;
            }

            default: {
                /* unknown format: print placeholder */
                terminal_putchar('?');
                break;
            }
        }
    }

    va_end(args);
}

/* wrapper expected by callers */
extern "C" void terminal_printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    terminal_vprintf(fmt, &args);
    va_end(args);
}
