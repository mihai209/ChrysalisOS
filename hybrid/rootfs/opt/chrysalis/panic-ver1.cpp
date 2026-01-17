// kernel/panic.cpp
// Beautiful ASCII-only panic screen for Chrysalis OS (compiles cleanly)
// Keeps the exact same public API and behavior
#include "panic.h"
#include "panic_sys.h"
#include <stddef.h>
#include <stdint.h>

#define VGA_MEM ((volatile uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25

static const uint8_t BG_BLUE = (1 << 4);
static const uint8_t WHITE = 15;
static const uint8_t YELLOW = 14;
static const uint8_t RED = 12;
static const uint8_t CYAN = 11;

static const uint8_t ATTR_TITLE    = BG_BLUE | YELLOW;
static const uint8_t ATTR_HEADER   = BG_BLUE | CYAN;
static const uint8_t ATTR_TEXT     = BG_BLUE | WHITE;
static const uint8_t ATTR_ERROR    = BG_BLUE | RED;
static const uint8_t ATTR_BAR_FILL = BG_BLUE | YELLOW;
static const uint8_t ATTR_BAR_EMPTY= BG_BLUE | WHITE;
static const uint8_t ATTR_YELLOW   = BG_BLUE | YELLOW;  // adăugat definiția lipsă

/* keyboard scancode (set 1) for 'm' press */
#define SC_M_PRESS 0x32

/* Weak multiboot symbols */
extern uint32_t multiboot_magic __attribute__((weak));
extern uint32_t multiboot_addr __attribute__((weak));
#define MULTIBOOT_MAGIC_CONST 0x2BADB002u

/* --- I/O helpers --- */
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* --- Serial output (mutat înainte de panic_render) --- */
static inline void serial_putc(char ch) {
    const uint16_t port = 0x3F8;
    while (!(inb(port + 5) & 0x20)) { /* spin */ }
    outb(port, (uint8_t)ch);
}
static void serial_write(const char *s) {
    if (!s) return;
    while (*s) serial_putc(*s++);
}

/* --- VGA helpers --- */
static size_t strlen_s(const char *s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len]) ++len;
    return len;
}

static void vga_putc_at(char c, int row, int col, uint8_t attr) {
    if (row < 0 || row >= VGA_H || col < 0 || col >= VGA_W) return;
    VGA_MEM[row * VGA_W + col] = ((uint16_t)attr << 8) | (uint8_t)c;
}

static void vga_write_at(const char *s, int row, int col, uint8_t attr) {
    if (!s) return;
    int c = col;
    while (*s && c < VGA_W) {
        vga_putc_at(*s++, row, c++, attr);
    }
}

static void vga_write_center(const char *s, int row, uint8_t attr) {
    if (!s) return;
    size_t len = strlen_s(s);
    int start = (len < VGA_W) ? (VGA_W - (int)len) / 2 : 0;
    vga_write_at(s, row, start, attr);
}

static void vga_clear(uint8_t attr) {
    uint16_t cell = ((uint16_t)attr << 8) | ' ';
    for (size_t i = 0; i < VGA_W * VGA_H; ++i) VGA_MEM[i] = cell;
}

static void draw_horizontal_line(int row, uint8_t attr) {
    vga_putc_at('+', row, 0, attr);
    for (int i = 1; i < VGA_W-1; ++i) vga_putc_at('-', row, i, attr);
    vga_putc_at('+', row, VGA_W-1, attr);
}

static void draw_border() {
    uint8_t attr = ATTR_HEADER;
    // top
    vga_putc_at('+', 0, 0, attr);
    for (int i = 1; i < VGA_W-1; ++i) vga_putc_at('-', 0, i, attr);
    vga_putc_at('+', 0, VGA_W-1, attr);
    // sides
    for (int r = 1; r < VGA_H-1; ++r) {
        vga_putc_at('|', r, 0, attr);
        vga_putc_at('|', r, VGA_W-1, attr);
    }
    // bottom
    vga_putc_at('+', VGA_H-1, 0, attr);
    for (int i = 1; i < VGA_W-1; ++i) vga_putc_at('-', VGA_H-1, i, attr);
    vga_putc_at('+', VGA_H-1, VGA_W-1, attr);

    draw_horizontal_line(8, attr);           // după mesaj
    draw_horizontal_line(VGA_H-5, attr);      // înainte de footer
}

/* --- Utilities --- */
static void u32_to_dec(uint32_t v, char *out, size_t outsz) {
    if (outsz == 0) return;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return; }
    char tmp[16]; int i = 0;
    while (v && i < 15) { tmp[i++] = '0' + (v % 10); v /= 10; }
    int j = 0;
    while (i > 0 && j + 1 < (int)outsz) out[j++] = tmp[--i];
    out[j] = '\0';
}

static uint32_t safe_percent(uint32_t used, uint32_t total) {
    if (total == 0) return 0;
    if (total <= (0xFFFFFFFFu / 100u)) return (used * 100u) / total;
    uint32_t div = total / 100u;
    if (div == 0) return used ? 100u : 0u;
    uint32_t p = used / div;
    return p > 100u ? 100u : p;
}

/* --- Multiboot fallback --- */
static void mb_extract(uint32_t *out_total_kb, const char **out_cmdline, const char **out_loader) {
    *out_total_kb = 0; *out_cmdline = NULL; *out_loader = NULL;
    uint32_t magic = (&multiboot_magic ? multiboot_magic : 0);
    uint32_t addr  = (&multiboot_addr  ? multiboot_addr  : 0);
    if (magic != MULTIBOOT_MAGIC_CONST || addr == 0) return;

    typedef struct { uint32_t flags, mem_lower, mem_upper, cmdline, boot_loader_name; } __attribute__((packed)) mb_t;
    mb_t *mb = (mb_t*)(uintptr_t)addr;

    if (mb->flags & 0x1) *out_total_kb = mb->mem_lower + mb->mem_upper;
    if (mb->flags & (1<<4)) *out_cmdline = (const char*)(uintptr_t)mb->cmdline;
    if (mb->flags & (1<<9)) *out_loader = (const char*)(uintptr_t)mb->boot_loader_name;
}

/* --- Reboot --- */
static void try_reboot(void) {
    outb(0x64, 0xFE);
    for (volatile int i = 0; i < 1000000; ++i) asm volatile("");
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) idt = {0, 0};
    asm volatile("lidt %0" : : "m"(idt));
    asm volatile("int $3");
    for (;;) asm volatile("hlt");
}

/* --- Bar drawing (ASCII only) --- */
static void draw_bar(int row, int col, int width, uint32_t percent) {
    if (percent > 100) percent = 100;
    int filled = (percent * width) / 100;

    vga_putc_at('[', row, col - 1, ATTR_BAR_FILL);
    for (int i = 0; i < width; ++i) {
        vga_putc_at(i < filled ? '#' : '-', row, col + i,
                i < filled ? ATTR_BAR_FILL : ATTR_BAR_EMPTY);
    }
    vga_putc_at(']', row, col + width, ATTR_BAR_FILL);
}

/* --- Main render function --- */
static void panic_render(const char *message, uint32_t eip, uint32_t cs, uint32_t eflags) {
    (void)eip; (void)cs; (void)eflags;

    vga_clear(ATTR_TEXT);
    draw_border();

    // Title box
    vga_write_center("================================================", 2, ATTR_TITLE);
    vga_write_center("  C H R Y S A L I S   O S                  ", 3, ATTR_TITLE);
    vga_write_center("          K E R N E L   P A N I C          ", 4, ATTR_TITLE);
    vga_write_center("================================================", 5, ATTR_TITLE);

    vga_write_center("A fatal error has occurred. System halted.", 6, ATTR_ERROR);

    // Panic message (wrapped)
    if (message && *message) {
        int row = 9;
        int col = 4;
        while (*message && row < 14) {
            int remaining = VGA_W - 8;
            const char *end = message;
            while (*end && remaining-- && *end != '\n') ++end;

            char line[80];
            size_t len = end - message;
            if (len > sizeof(line)-1) len = sizeof(line)-1;
            for (size_t i = 0; i < len; ++i) line[i] = message[i];
            line[len] = '\0';

            vga_write_at(line, row++, col, ATTR_TEXT);
            message += len;
            while (*message == '\n') ++message;
        }
    } else {
        vga_write_center("(no panic message provided)", 10, ATTR_TEXT);
    }

    // System info (right panel)
    uint32_t total_kb = panic_sys_total_ram_kb();
    uint32_t free_kb  = panic_sys_free_ram_kb();
    uint32_t sto_total = panic_sys_storage_total_kb();
    uint32_t sto_free  = panic_sys_storage_free_kb();
    const char *cpu = panic_sys_cpu_str();
    uint32_t uptime = panic_sys_uptime_seconds();

    if (total_kb == 0 || !cpu || !cpu[0]) {
        uint32_t mb_total = 0;
        const char *mb_cmd = NULL, *mb_loader = NULL;
        mb_extract(&mb_total, &mb_cmd, &mb_loader);
        if (total_kb == 0 && mb_total) total_kb = mb_total;
        if (!cpu || !cpu[0]) cpu = mb_cmd && mb_cmd[0] ? mb_cmd : mb_loader;
    }

    int info_row = 9;
    int info_col = 44;

    vga_write_at("SYSTEM INFO", info_col, info_row++, ATTR_HEADER);
    info_row++;

    if (total_kb) {
        char buf[16]; u32_to_dec(total_kb, buf, sizeof(buf));
        vga_write_at("RAM Total :", info_col, info_row, ATTR_TEXT);
        vga_write_at(buf, info_col + 12, info_row++, ATTR_TEXT);
    }
    if (free_kb) {
        char buf[16]; u32_to_dec(free_kb, buf, sizeof(buf));
        vga_write_at("RAM Free  :", info_col, info_row, ATTR_TEXT);
        vga_write_at(buf, info_col + 12, info_row++, ATTR_TEXT);
    }
    if (total_kb && free_kb && total_kb >= free_kb) {
        uint32_t used = total_kb - free_kb;
        uint32_t pct = safe_percent(used, total_kb);
        char pctstr[8]; u32_to_dec(pct, pctstr, sizeof(pctstr));
        vga_write_at("RAM Usage :", info_col, info_row++, ATTR_TEXT);
        draw_bar(info_row, info_col + 12, 20, pct);
        vga_write_at(pctstr, info_col + 34, info_row, ATTR_TEXT);
        vga_write_at("%", info_col + 37, info_row++, ATTR_TEXT);
    }

    if (sto_total) {
        char buf[16]; u32_to_dec(sto_total, buf, sizeof(buf));
        vga_write_at("Storage Tot:", info_col, info_row, ATTR_TEXT);
        vga_write_at(buf, info_col + 13, info_row++, ATTR_TEXT);
    }
    if (sto_free) {
        char buf[16]; u32_to_dec(sto_free, buf, sizeof(buf));
        vga_write_at("Storage Free:", info_col, info_row, ATTR_TEXT);
        vga_write_at(buf, info_col + 14, info_row++, ATTR_TEXT);
    }
    if (sto_total && sto_free && sto_total >= sto_free) {
        uint32_t used = sto_total - sto_free;
        uint32_t pct = safe_percent(used, sto_total);
        char pctstr[8]; u32_to_dec(pct, pctstr, sizeof(pctstr));
        vga_write_at("Storage Use:", info_col, info_row++, ATTR_TEXT);
        draw_bar(info_row, info_col + 13, 20, pct);
        vga_write_at(pctstr, info_col + 34, info_row, ATTR_TEXT);
        vga_write_at("%", info_col + 37, info_row++, ATTR_TEXT);
    }

    if (cpu && cpu[0]) {
        vga_write_at("Boot/Cmd :", info_col, info_row, ATTR_TEXT);
        size_t len = strlen_s(cpu);
        if (len > 28) len = 28;
        char truncated[29];
        for (size_t i = 0; i < len; ++i) truncated[i] = cpu[i];
        truncated[len] = '\0';
        vga_write_at(truncated, info_col + 11, info_row++, ATTR_TEXT);
    }

    if (uptime) {
        char buf[16]; u32_to_dec(uptime, buf, sizeof(buf));
        vga_write_at("Uptime (s):", info_col, info_row, ATTR_TEXT);
        vga_write_at(buf, info_col + 12, info_row++, ATTR_TEXT);
    }

    // Footer
    vga_write_center("Press [M] to attempt reboot", VGA_H-3, ATTR_HEADER);
    vga_write_center("Website: https://chrysalisos.netlify.app/", VGA_H-2, ATTR_YELLOW);

    // Serial mirror
    serial_write("\n*** CHRYSALIS KERNEL PANIC ***\n");
    if (message && *message) serial_write(message);
    serial_write("\n");
}

/* --- Public API --- */
extern "C" void panic(const char *message) {
    panic_ex(message, 0, 0, 0);
}

extern "C" void panic_ex(const char *message, uint32_t eip, uint32_t cs, uint32_t eflags) {
    asm volatile("cli");
    panic_render(message, eip, cs, eflags);
    for (;;) {
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            uint8_t sc = inb(0x60);
            if (sc == SC_M_PRESS) try_reboot();
        }
        for (volatile int i = 0; i < 10000; ++i) asm volatile("");
    }
}