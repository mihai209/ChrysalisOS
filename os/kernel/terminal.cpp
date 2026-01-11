// kernel/terminal/terminal.cpp
#include "terminal.h"
#include "video/fb_console.h"
#include <stdarg.h>
#include <stdint.h>
#include "vt/vt.h"
#include "drivers/serial.h"
#include "video/surface.h"
#include "string.h"
#include "video/font8x16.h"

extern "C" void serial(const char *fmt, ...);

static uint16_t* vga = (uint16_t*)0xB8000;
static int row = 0;
static int col = 0;
static const uint8_t color = 0x0F;
static bool use_fb_console = false;
static terminal_mode_t term_mode = TERMINAL_MODE_VGA;
static surface_t* term_surface = NULL;
static int term_x = 0;
static int term_y = 0;
static int term_w = 0;
static int term_h = 0;
static bool term_dirty = true;
static bool term_rendering_enabled = true;

/* Redirection State */
static char* capture_buf = 0;
static size_t capture_max = 0;
static size_t* capture_written = 0;

static const char* input_buf = 0;
static size_t input_len = 0;
static size_t input_pos = 0;

static void scroll() {
    if (row < 25) return;

    for (int y = 1; y < 25; y++)
        for (int x = 0; x < 80; x++)
            vga[(y - 1) * 80 + x] = vga[y * 80 + x];

    for (int x = 0; x < 80; x++)
        vga[24 * 80 + x] = ' ' | (color << 8);

    row = 24;
}

/* Window Mode Helpers */
static void term_window_scroll() {
    if (!term_surface) return;
    /* Move pixels up by 16 rows */
    uint32_t pitch = term_surface->width; // 32bpp assumed
    uint32_t* pixels = term_surface->pixels;
    int total_rows = term_h / 16;
    
    /* Scroll only the rect area */
    for (int y = 1; y < total_rows; y++) {
        int dst_y = term_y + (y - 1) * 16;
        int src_y = term_y + y * 16;
        
        /* Move 16 scanlines */
        for (int i = 0; i < 16; i++) {
            uint32_t* dst_row = pixels + (dst_y + i) * pitch + term_x;
            uint32_t* src_row = pixels + (src_y + i) * pitch + term_x;
            memmove(dst_row, src_row, term_w * 4);
        }
    }

    /* Clear bottom line */
    int bottom_y = term_y + (total_rows - 1) * 16;
    for (int i = 0; i < 16; i++) {
        uint32_t* row_ptr = pixels + (bottom_y + i) * pitch + term_x;
        for (int x = 0; x < term_w; x++) row_ptr[x] = 0xFFFFFFFF; /* White */
    }
    
    row = total_rows - 1;
}

static void term_window_putpixel(int x, int y, uint32_t color) {
    if (!term_surface) return;
    int px = term_x + x;
    int py = term_y + y;
    if (px < 0 || py < 0 || px >= (int)term_surface->width || py >= (int)term_surface->height) return;
    if (x >= term_w || y >= term_h) return;
    
    term_surface->pixels[py * term_surface->width + px] = color;
}

static void term_window_draw_char(char c, int x, int y) {
    const uint8_t* glyph = &font8x16[(uint8_t)c * 16];
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            /* Windows 1.0 Console: Black text (0xFF000000) on White background (0xFFFFFFFF) */
            uint32_t color = (glyph[i] & (1 << (7-j))) ? 0xFF000000 : 0xFFFFFFFF;
            term_window_putpixel(x + j, y + i, color);
        }
    }
}

extern "C" void terminal_set_backend_fb(bool active) {
    use_fb_console = active;
    term_mode = active ? TERMINAL_MODE_FB : TERMINAL_MODE_VGA;
    serial("[TERM] Mode set to %s\n", active ? "FB" : "VGA");
}

extern "C" void terminal_set_rendering(bool enabled) {
    term_rendering_enabled = enabled;
}

extern "C" bool terminal_is_dirty(void) {
    return term_dirty;
}

extern "C" void terminal_clear_dirty(void) {
    term_dirty = false;
}

extern "C" void terminal_clear() {
    if (term_mode == TERMINAL_MODE_WINDOW && term_surface) {
        /* Clear only the rect */
        uint32_t pitch = term_surface->width;
        uint32_t* pixels = term_surface->pixels;
        for (int y = 0; y < term_h; y++) {
            uint32_t* row_ptr = pixels + (term_y + y) * pitch + term_x;
            for (int x = 0; x < term_w; x++) row_ptr[x] = 0xFFFFFFFF;
        }
        row = 0; col = 0;
        term_dirty = true;
        return;
    }
    if (use_fb_console) {
        fb_cons_clear();
        return;
    }

    for (int i = 0; i < 80 * 25; i++)
        vga[i] = ' ' | (color << 8);

    row = 0;
    col = 0;
}

extern "C" void terminal_putchar(char c) {
    /* Global rendering lock for GUI mode */
    if (!term_rendering_enabled) return;

    /* Handle Output Redirection (Piping) */
    if (capture_buf) {
        if (capture_written && *capture_written < capture_max) {
            capture_buf[(*capture_written)++] = c;
        }
        return; /* Do not print to screen when piped */
    }

    if (term_mode == TERMINAL_MODE_WINDOW && term_surface) {
        int max_cols = term_w / 8;
        int max_rows = term_h / 16;

        if (c == '\n') {
            row++; col = 0;
            if (row >= max_rows) term_window_scroll();
            term_dirty = true;
            return;
        }
        if (c == '\r') { col = 0; return; }
        if (c == '\b') {
            if (col > 0) {
                col--;
                term_window_draw_char(' ', col * 8, row * 16);
            }
            term_dirty = true;
            return;
        }

        term_window_draw_char(c, col * 8, row * 16);
        col++;
        if (col >= max_cols) {
            col = 0; row++;
            if (row >= max_rows) term_window_scroll();
        }
        term_dirty = true;
        return;
    }

    if (use_fb_console) {
        /* Route through VT subsystem to update active buffer */
        vt_putc(c);
        return;
    }

    if (c == '\n') {
        row++;
        col = 0;
        scroll();
        return;
    }

    if (c == '\r') {
        col = 0;
        return;
    }

    if (c == '\b') {
        if (col > 0) col--;
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
    if (term_mode == TERMINAL_MODE_WINDOW) {
        while (*s) terminal_putchar(*s++);
        return;
    }
    if (use_fb_console) {
        fb_cons_puts(s);
        return;
    }

    while (*s)
        terminal_putchar(*s++);
}

extern "C" void terminal_init() {
    terminal_clear();
}

extern "C" void terminal_set_surface(surface_t* s) {
    if (s) {
        term_surface = s;
        term_mode = TERMINAL_MODE_WINDOW;
        term_x = 0;
        term_y = 0;
        term_w = s->width;
        term_h = s->height;
        row = 0;
        col = 0;
        term_dirty = true;
        serial("[TERM] Surface attached (0x%x). Mode: WINDOW\n", s);
    } else {
        term_surface = NULL;
        term_mode = use_fb_console ? TERMINAL_MODE_FB : TERMINAL_MODE_VGA;
        row = 0;
        col = 0;
        term_dirty = true;
        serial("[TERM] Surface detached. Mode: TEXT/FB\n");
    }
}

extern "C" void terminal_set_rect(int x, int y, int w, int h) {
    term_x = x;
    term_y = y;
    term_w = w;
    term_h = h;
    row = 0;
    col = 0;
    term_dirty = true;
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

        /* Parse flags and width */
        int pad_zero = 0;
        int width = 0;
        
        while (*fmt == '0') {
            pad_zero = 1;
            fmt++;
        }
        
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        
        /* Parse type */
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
                /* Simple padding support for decimal not fully implemented here to save space, 
                   but we consume the modifiers so they don't print garbage. */
                if (v < 0) { terminal_putchar('-'); v = -v; }
                print_uint32((uint32_t)v);
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
                
                /* Manual hex formatting with padding */
                char buf[16];
                int i = 0;
                const char* digits = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                
                if (v == 0) {
                    buf[i++] = '0';
                } else {
                    while (v) {
                        buf[i++] = digits[v & 0xF];
                        v >>= 4;
                    }
                }
                
                /* Pad */
                while (i < width) {
                    buf[i++] = pad_zero ? '0' : ' ';
                }
                
                /* Print reversed */
                while (i > 0) {
                    terminal_putchar(buf[--i]);
                }
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

/* ---------- NEW: print hex trimmed (no leading zeros) ---------- */
/* prints hex without 0x; trims leading zeros so 0x00008086 -> "8086" */
extern "C" void terminal_writehex(uint32_t v)
{
    const char* hex = "0123456789ABCDEF";
    bool started = false;

    for (int i = 7; i >= 0; i--) {
        uint8_t nib = (v >> (i * 4)) & 0xF;
        if (nib != 0 || started || i == 0) {
            started = true;
            terminal_putchar(hex[nib]);
        }
    }
}

/* ===== Piping Implementation ===== */

extern "C" void terminal_start_capture(char* buf, size_t max_len, size_t* out_len) {
    capture_buf = buf;
    capture_max = max_len;
    capture_written = out_len;
    if (capture_written) *capture_written = 0;
}

extern "C" void terminal_end_capture(void) {
    capture_buf = 0;
    capture_max = 0;
    capture_written = 0;
}

extern "C" void terminal_set_input(const char* buf, size_t len) {
    input_buf = buf;
    input_len = len;
    input_pos = 0;
}

extern "C" int terminal_read_char(void) {
    if (input_buf && input_pos < input_len) {
        return (unsigned char)input_buf[input_pos++];
    }
    return -1; /* EOF */
}
