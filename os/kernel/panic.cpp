// kernel/panic.cpp
#include "panic.h"
#include "panic_sys.h"

// === Headere freestanding sigure (fără libc) ===
typedef unsigned int       size_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;

// === Implementare fallback pentru strlen (înlocuiește <string.h>) ===
static size_t strlen(const char* str) {
    const char* s = str;
    while (*s) ++s;
    return s - str;
}

// === Restul codului rămâne identic, doar cu ajustări minore ===

#define VGA_MEM ((volatile uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25

// Schema de culori modernă 2025 - Dark Mode + Aero Glass vibe
enum {
    COLOR_BG_DARK   = 0,   // Negru
    COLOR_WIN       = 8,   // Gri închis
    COLOR_GLASS     = 7,   // Gri deschis (highlight)
    COLOR_BORDER    = 11,  // Cyan
    COLOR_SHADOW    = 0,
    COLOR_TXT       = 15,
    COLOR_LBL       = 14,
    COLOR_VAL       = 10,
    COLOR_ERR       = 12,
    COLOR_ACCENT    = 9
};

static inline uint8_t attr(uint8_t fg, uint8_t bg) { return (bg << 4) | (fg & 0x0F); }

/* --- I/O & Serial --- */
static inline void outb(uint16_t port, uint8_t val) { asm volatile("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }

static void serial_print(const char* s) {
    for (int i = 0; s[i]; i++) {
        while (!(inb(0x3F8 + 5) & 0x20));
        outb(0x3F8, s[i]);
    }
}

/* --- Primitve --- */
static void put_c(int x, int y, char c, uint8_t a) {
    if (x >= 0 && x < VGA_W && y >= 0 && y < VGA_H)
        VGA_MEM[y * VGA_W + x] = ((uint16_t)a << 8) | (uint8_t)c;
}

static void draw_rect(int x, int y, int w, int h, uint8_t a) {
    for (int i = y; i < y + h; i++)
        for (int j = x; j < x + w; j++) put_c(j, i, ' ', a);
}

static int draw_string_wrap(int x, int y, int max_w, const char* s, uint8_t a) {
    int cur_x = x;
    int cur_y = y;
    while (*s) {
        if (*s == '\n' || cur_x >= x + max_w) {
            cur_x = x;
            cur_y++;
            if (*s == '\n') { s++; continue; }
        }
        put_c(cur_x++, cur_y, *s++, a);
    }
    return cur_y;
}

/* --- Fereastră modernă --- */
static void draw_window_modern(int x, int y, int w, int h, const char* title) {
    draw_rect(x + 2, y + 2, w, h, attr(COLOR_TXT, COLOR_SHADOW));
    draw_rect(x, y, w, h, attr(COLOR_TXT, COLOR_WIN));

    for (int i = x; i < x + w; i++) {
        put_c(i, y + 1, ' ', attr(COLOR_TXT, COLOR_GLASS));
        put_c(i, y + 2, ' ', attr(COLOR_TXT, COLOR_GLASS));
    }

    uint8_t b_a = attr(COLOR_BORDER, COLOR_WIN);
    for (int i = x + 2; i < x + w - 2; i++) {
        put_c(i, y, 196, b_a);
        put_c(i, y + h - 1, 196, b_a);
    }
    for (int i = y + 2; i < y + h - 2; i++) {
        put_c(x, i, 179, b_a);
        put_c(x + w - 1, i, 179, b_a);
    }

    put_c(x + 1, y, 218, b_a); put_c(x + w - 2, y, 191, b_a);
    put_c(x + 1, y + h - 1, 192, b_a); put_c(x + w - 2, y + h - 1, 217, b_a);
    put_c(x, y + 1, 179, b_a); put_c(x + w - 1, y + 1, 179, b_a);
    put_c(x, y + h - 2, 179, b_a); put_c(x + w - 1, y + h - 2, 179, b_a);

    size_t title_len = strlen(title);
    int title_x = x + (w - (int)title_len) / 2;
    draw_string_wrap(title_x, y + 1, w, title, attr(COLOR_BG_DARK, COLOR_BORDER));
}

/* --- Progress bar --- */
static void draw_progress_modern(int x, int y, int w, uint32_t pct, const char* lbl) {
    draw_string_wrap(x, y, 20, lbl, attr(COLOR_LBL, COLOR_WIN));

    int bar_x = x + 16;
    int bar_w = w - 16;
    if (pct > 100) pct = 100;
    int filled = (pct * bar_w) / 100;

    put_c(bar_x - 1, y, '[', attr(COLOR_BORDER, COLOR_WIN));
    put_c(bar_x + bar_w, y, ']', attr(COLOR_BORDER, COLOR_WIN));

    uint8_t fill_attr = attr(pct >= 80 ? COLOR_ERR : COLOR_VAL, COLOR_WIN);

    for (int i = 0; i < bar_w; i++) {
        char block;
        if (i < filled * 0.6)       block = 219;
        else if (i < filled * 0.8)  block = 178;
        else if (i < filled)        block = 177;
        else                        block = 176;
        put_c(bar_x + i, y, block, fill_attr);
    }
}

/* --- u32 to string --- */
static void u32_ptr(uint32_t v, char* b) {
    char t[12];
    int i = 0;
    if (!v) { b[0]='0'; b[1]=0; return; }
    while (v) { t[i++] = (v % 10) + '0'; v /= 10; }
    int j = 0;
    while (i > 0) b[j++] = t[--i];
    b[j] = 0;
}

/* --- Render panic --- */
extern "C" void panic_render_pretty(const char* msg) {
    draw_rect(0, 0, VGA_W, VGA_H, attr(COLOR_TXT, COLOR_BG_DARK));
    draw_window_modern(6, 3, 68, 19, "  CRITICAL SYSTEM FAILURE  ");

    draw_string_wrap(9, 6, 60, "Kernel panic - unrecoverable error", attr(COLOR_TXT, COLOR_WIN));
    int last_y = draw_string_wrap(9, 8, 62, msg ? msg : "No error description available.", attr(COLOR_ERR, COLOR_WIN));

    int stats_y = last_y + 3;
    draw_string_wrap(9, stats_y, 40, "SYSTEM DIAGNOSTICS", attr(COLOR_ACCENT, COLOR_WIN));
    for (int i = 8; i < 72; i++) {
        put_c(i, stats_y + 1, 196, attr(COLOR_BORDER, COLOR_WIN));
    }

    uint32_t tot = panic_sys_total_ram_kb();
    uint32_t fr = panic_sys_free_ram_kb();
    if (tot > 0) {
        uint32_t used_pct = ((tot - fr) * 100) / tot;
        draw_progress_modern(9, stats_y + 3, 58, used_pct, "Memory Usage:");
    }

    const char* cpu = panic_sys_cpu_str();
    draw_string_wrap(9, stats_y + 5, 20, "Processor:", attr(COLOR_LBL, COLOR_WIN));
    draw_string_wrap(28, stats_y + 5, 40, cpu ? cpu : "Generic x86", attr(COLOR_VAL, COLOR_WIN));

    char upt[16];
    u32_ptr(panic_sys_uptime_seconds(), upt);
    draw_string_wrap(9, stats_y + 6, 20, "Uptime (seconds):", attr(COLOR_LBL, COLOR_WIN));
    draw_string_wrap(28, stats_y + 6, 15, upt, attr(COLOR_VAL, COLOR_WIN));

    draw_string_wrap(12, 21, 60, "Chrysalis OS | https://chrysalisos.netlify.app", attr(COLOR_GLASS, COLOR_WIN));
    draw_string_wrap(22, 22, 40, "[M] Repornire de urgenta!", attr(COLOR_TXT, COLOR_ACCENT));
}

extern "C" void try_reboot() {
    outb(0x64, 0xFE);
    for (volatile int i = 0; i < 100000; i++);
    struct { uint16_t l; uint32_t b; } __attribute__((packed)) idt = {0,0};
    asm volatile("lidt %0; int $3" : : "m"(idt));
}

extern "C" void panic_ex(const char *msg, uint32_t eip, uint32_t cs, uint32_t eflags) {
    (void)eip; (void)cs; (void)eflags;
    asm volatile("cli");

    serial_print("\n!!! KERNEL PANIC !!!\n");
    if (msg) serial_print(msg);
    serial_print("\n");

    panic_render_pretty(msg);

    for (;;) {
        if (inb(0x64) & 1) {
            if (inb(0x60) == 0x32) {
                try_reboot();
            }
        }
    }
}

extern "C" void panic(const char *msg) {
    panic_ex(msg, 0, 0, 0);
}