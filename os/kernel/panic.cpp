#include "panic.h"
#include "panic_sys.h"
#include <stddef.h>
#include <stdint.h>

#define VGA_MEM ((volatile uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25

// Schema de culori "Chrysalis Premium"
enum {
    COLOR_BG      = 1,  // Albastru inchis
    COLOR_SHADOW  = 0,  // Negru (umbra)
    COLOR_WIN     = 1,  // Albastru
    COLOR_BORDER  = 11, // Cyan deschis
    COLOR_TXT     = 15, // Alb pur
    COLOR_LBL     = 14, // Galben (etichete)
    COLOR_VAL     = 10, // Verde (valori ok)
    COLOR_ERR     = 12  // Rosu (eroare)
};

static inline uint8_t attr(uint8_t fg, uint8_t bg) { return (bg << 4) | (fg & 0x0F); }

/* --- I/O & Serial Fallback --- */
static inline void outb(uint16_t port, uint8_t val) { asm volatile("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }

static void serial_print(const char* s) {
    for (int i = 0; s[i]; i++) {
        while (!(inb(0x3F8 + 5) & 0x20));
        outb(0x3F8, s[i]);
    }
}

/* --- Randare Primitiva --- */
static void put_c(int x, int y, char c, uint8_t a) {
    if (x >= 0 && x < VGA_W && y >= 0 && y < VGA_H)
        VGA_MEM[y * VGA_W + x] = ((uint16_t)a << 8) | (uint8_t)c;
}

static void draw_rect(int x, int y, int w, int h, uint8_t a) {
    for (int i = y; i < y + h; i++)
        for (int j = x; j < x + w; j++) put_c(j, i, ' ', a);
}

// Scriere Multi-line cu detectare de margini
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
    return cur_y; // Returneaza ultimul rand folosit
}

/* --- UI Components --- */
static void draw_window(int x, int y, int w, int h, const char* title) {
    // Umbra ferestrei (efect 3D)
    draw_rect(x + 1, y + 1, w, h, attr(0, COLOR_SHADOW));
    // Corpul ferestrei
    draw_rect(x, y, w, h, attr(COLOR_TXT, COLOR_WIN));
    // Border dublu (CP437)
    uint8_t b_a = attr(COLOR_BORDER, COLOR_WIN);
    for (int i = x + 1; i < x + w - 1; i++) { put_c(i, y, (char)205, b_a); put_c(i, y + h - 1, (char)205, b_a); }
    for (int i = y + 1; i < y + h - 1; i++) { put_c(x, i, (char)186, b_a); put_c(x + w - 1, i, (char)186, b_a); }
    put_c(x, y, (char)201, b_a); put_c(x + w - 1, y, (char)187, b_a);
    put_c(x, y + h - 1, (char)200, b_a); put_c(x + w - 1, y + h - 1, (char)188, b_a);
    // Titlu
    draw_string_wrap(x + (w / 2) - 10, y, 30, title, attr(COLOR_WIN, COLOR_BORDER));
}

static void draw_progress(int x, int y, int w, uint32_t pct, const char* lbl) {
    draw_string_wrap(x, y, 20, lbl, attr(COLOR_LBL, COLOR_WIN));
    int bar_x = x + 12;
    if (pct > 100) pct = 100;
    int filled = (pct * (w - 12)) / 100;
    for (int i = 0; i < (w - 12); i++) {
        put_c(bar_x + i, y, (i < filled) ? (char)219 : (char)176, attr(COLOR_LBL, COLOR_WIN));
    }
}

/* --- Logic --- */
static void u32_ptr(uint32_t v, char* b) {
    char t[12]; int i = 0;
    if (!v) { b[0]='0'; b[1]=0; return; }
    while (v) { t[i++] = (v % 10) + '0'; v /= 10; }
    int j = 0; while (i > 0) b[j++] = t[--i]; b[j] = 0;
}

extern "C" void panic_render_pretty(const char* msg) {
    // 1. Fundal curat
    draw_rect(0, 0, VGA_W, VGA_H, attr(7, 0));
    
    // 2. Fereastra principala
    draw_window(5, 2, 70, 20, " CRITICAL SYSTEM FAILURE ");

    // 3. Mesaj Multi-line
    draw_string_wrap(8, 4, 60, "The kernel has encountered an unrecoverable error:", attr(COLOR_TXT, COLOR_WIN));
    int last_y = draw_string_wrap(8, 6, 64, msg ? msg : "No error description provided.", attr(COLOR_ERR, COLOR_WIN));

    // 4. Statisti (Dinamice sub mesaj)
    int stats_y = (last_y < 10) ? 11 : last_y + 2;
    draw_rect(7, stats_y - 1, 66, 1, attr(COLOR_BORDER, COLOR_WIN)); // Linie separatoare
    for(int i=7; i<73; i++) put_c(i, stats_y-1, (char)196, attr(COLOR_BORDER, COLOR_WIN));

    draw_string_wrap(8, stats_y, 20, "SYSTEM STATUS:", attr(COLOR_LBL, COLOR_WIN));
    
    // RAM
    uint32_t tot = panic_sys_total_ram_kb();
    uint32_t fr = panic_sys_free_ram_kb();
    if (tot > 0) {
        uint32_t used = ((tot - fr) * 100) / tot;
        draw_progress(8, stats_y + 2, 40, used, "Memory:");
    }

    // CPU Info
    const char* cpu = panic_sys_cpu_str();
    draw_string_wrap(8, stats_y + 4, 15, "Processor:", attr(COLOR_LBL, COLOR_WIN));
    draw_string_wrap(20, stats_y + 4, 50, cpu ? cpu : "Generic x86", attr(COLOR_VAL, COLOR_WIN));

    // Uptime
    char upt[12]; u32_ptr(panic_sys_uptime_seconds(), upt);
    draw_string_wrap(8, stats_y + 5, 15, "Uptime(s):", attr(COLOR_LBL, COLOR_WIN));
    draw_string_wrap(20, stats_y + 5, 10, upt, attr(COLOR_VAL, COLOR_WIN));

    // 5. Footer Interactive
    draw_string_wrap(15, 20, 50, "Visit: https://chrysalisos.netlify.app/", attr(8, COLOR_WIN));
    draw_rect(6, 21, 68, 1, attr(0, COLOR_LBL));
    draw_string_wrap(22, 21, 40, " [M] ATTEMPT EMERGENCY REBOOT ", attr(0, COLOR_LBL));
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
    
    // Log pe serial inainte de orice (daca VGA e corupt)
    serial_print("\n!!! KERNEL PANIC !!!\n");
    if(msg) serial_print(msg);
    serial_print("\n");

    panic_render_pretty(msg);

    for (;;) {
        if (inb(0x64) & 1) {
            if (inb(0x60) == 0x32) try_reboot();
        }
    }
}

extern "C" void panic(const char *msg) {
    panic_ex(msg, 0, 0, 0);
}