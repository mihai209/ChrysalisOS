// kernel/panic.cpp
// Compact, improved panic screen for Chrysalis OS
// - hides unknown fields
// - shows [M] Reboot hint
// - prints Website
// - reads panic_sys_* first, then Multiboot as fallback
// - freestanding, avoids libgcc 64-bit helpers

#include "panic.h"
#include "panic_sys.h"
#include <stddef.h>
#include <stdint.h>

#define VGA_MEM ((volatile uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25

static const uint8_t ATTR_BG_BLUE = (1 << 4);
static const uint8_t ATTR_WHITE_ON_BLUE = ATTR_BG_BLUE | 15;
static const uint8_t ATTR_YELLOW_ON_BLUE = ATTR_BG_BLUE | 14;
static const uint8_t ATTR_RED_ON_BLUE = ATTR_BG_BLUE | 12;

/* keyboard scancode (set 1) for 'm' press */
#define SC_M_PRESS 0x32

/* Weak multiboot symbols (optional, provided by boot code if you set them) */
extern uint32_t multiboot_magic __attribute__((weak));
extern uint32_t multiboot_addr  __attribute__((weak));
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

/* --- VGA helpers --- */
static size_t strlen_s(const char *s) {
    if (!s) return 0;
    const char *p = s;
    while (*p) ++p;
    return (size_t)(p - s);
}
static void vga_putc_at(char c, int row, int col, uint8_t attr) {
    if (row < 0 || row >= VGA_H || col < 0 || col >= VGA_W) return;
    volatile uint16_t *v = VGA_MEM;
    v[row * VGA_W + col] = ((uint16_t)attr << 8) | (uint8_t)c;
}
static void vga_write_at(const char *s, int row, int col, uint8_t attr) {
    if (!s) return;
    if (row < 0 || row >= VGA_H) return;
    if (col < 0) col = 0;
    volatile uint16_t *v = VGA_MEM;
    int pos = row * VGA_W + col;
    int c = col;
    while (*s && c < VGA_W) {
        v[pos++] = ((uint16_t)attr << 8) | (uint8_t)(*s++);
        ++c;
    }
}
static void vga_write_center(const char *s, int row, uint8_t attr) {
    if (!s) return;
    int len = (int)strlen_s(s);
    int start = 0;
    if (len < VGA_W) start = (VGA_W - len) / 2;
    vga_write_at(s, row, start, attr);
}
static void vga_write_wrap(const char *s, int row, int col, uint8_t attr) {
    if (!s) return;
    if (row < 0 || row >= VGA_H) return;
    volatile uint16_t *v = VGA_MEM;
    int r = row;
    int c = col;
    int pos = r * VGA_W + c;
    while (*s && r < VGA_H) {
        if (*s == '\n' || c >= VGA_W) {
            ++r; c = col; pos = r * VGA_W + c;
            if (*s == '\n') { ++s; continue; }
            if (r >= VGA_H) break;
            continue;
        }
        v[pos++] = ((uint16_t)attr << 8) | (uint8_t)(*s++);
        ++c;
    }
}

/* --- Serial fallback --- */
static inline void serial_putc(char ch) {
    const uint16_t port = 0x3F8;
    while (!(inb(port + 5) & 0x20)) { /* spin */ }
    outb(port, (uint8_t)ch);
}
static void serial_write(const char *s) {
    if (!s) return;
    while (*s) serial_putc(*s++);
}

/* --- Utilities --- */
static void u32_to_dec(uint32_t v, char *out, size_t outsz) {
    if (outsz == 0) return;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return; }
    char tmp[16]; int ti = 0;
    while (v && ti < (int)sizeof(tmp)-1) { tmp[ti++] = '0' + (v % 10); v /= 10; }
    size_t i = 0;
    while (ti > 0 && i + 1 < outsz) out[i++] = tmp[--ti];
    out[i] = '\0';
}

/* safe percent calculation avoiding 64-bit multiply */
static uint32_t safe_percent(uint32_t used, uint32_t total) {
    if (total == 0) return 0;
    if (total <= (0xFFFFFFFFu / 100u)) {
        return (used * 100u) / total;
    } else {
        uint32_t div = total / 100u;
        if (div == 0) return (used ? 100u : 0u);
        uint32_t p = used / div;
        if (p > 100u) p = 100u;
        return p;
    }
}

/* --- Minimal Multiboot v1 struct (we read only a few fields) --- */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms_a;
    uint32_t syms_b;
    uint32_t syms_c;
    uint32_t syms_d;
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed)) multiboot_info_m1_t;

/* --- Extract a few Multiboot fields as fallback --- */
static void mb_extract(uint32_t *out_total_kb, const char **out_cmdline, const char **out_loader) {
    *out_total_kb = 0;
    *out_cmdline = NULL;
    *out_loader = NULL;
    uint32_t magic = 0;
    uint32_t addr  = 0;
    /* use weak symbols if present */
    if (&multiboot_magic) magic = multiboot_magic;
    if (&multiboot_addr)  addr  = multiboot_addr;
    if (magic != MULTIBOOT_MAGIC_CONST || addr == 0) return;
    multiboot_info_m1_t *mb = (multiboot_info_m1_t*)(uintptr_t)addr;
    if (!mb) return;
    if (mb->flags & 0x1) {
        uint32_t sum = mb->mem_lower + mb->mem_upper; /* both already KB */
        *out_total_kb = sum;
    }
    if (mb->flags & (1 << 4)) {
        *out_cmdline = (const char*)(uintptr_t)mb->cmdline;
    }
    if (mb->flags & (1 << 9)) {
        *out_loader = (const char*)(uintptr_t)mb->boot_loader_name;
    }
}

/* --- Reboot attempt: KBC reset then triple-fault fallback --- */
static void try_reboot(void) {
    outb(0x64, 0xFE);
    for (volatile int i = 0; i < 1000000; ++i) { asm volatile(""); }
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) idt_ptr = { 0, 0 };
    asm volatile("lidt %0" : : "m"(idt_ptr));
    asm volatile("int $3");
    for (;;) asm volatile("hlt");
}

/* --- small bar drawing (compact) --- */
static void draw_bar(int row, int col, int width, uint32_t percent, uint8_t attr_fill, uint8_t attr_empty) {
    if (width <= 0) return;
    if (percent > 100u) percent = 100u;
    int filled = (percent * width) / 100;
    for (int i = 0; i < width; ++i) {
        vga_putc_at((i < filled) ? '#' : '-', row, col + i, (i < filled) ? attr_fill : attr_empty);
    }
}

/* --- Render panic screen (compact layout) --- */
static void panic_render(const char *message, uint32_t eip, uint32_t cs, uint32_t eflags) {
    (void)eip; (void)cs; (void)eflags;

    /* clear screen with blue */
    volatile uint16_t *v = VGA_MEM;
    uint16_t cell = ((uint16_t)ATTR_WHITE_ON_BLUE << 8) | (uint8_t)' ';
    for (size_t i = 0; i < VGA_W * VGA_H; ++i) v[i] = cell;

    /* header */
    vga_write_center("CHRYSALIS - KERNEL PANIC", 0, ATTR_YELLOW_ON_BLUE);
    vga_write_center("A fatal error occurred. System halted.", 1, ATTR_WHITE_ON_BLUE);

    /* DETAILS */
    vga_write_at("DETAILS:", 3, 1, ATTR_RED_ON_BLUE);
    if (message && *message) vga_write_wrap(message, 4, 1, ATTR_WHITE_ON_BLUE);
    else vga_write_at("(no message provided)", 4, 1, ATTR_WHITE_ON_BLUE);

    /* gather info (prefer panic_sys) */
    uint32_t total_kb = panic_sys_total_ram_kb();
    uint32_t free_kb  = panic_sys_free_ram_kb();
    uint32_t storage_total = panic_sys_storage_total_kb();
    uint32_t storage_free  = panic_sys_storage_free_kb();
    const char *cpu = panic_sys_cpu_str();
    uint32_t uptime = panic_sys_uptime_seconds();

    /* fallback to multiboot if needed */
    if ((total_kb == 0) || (!cpu || cpu[0] == '\0')) {
        uint32_t mb_total = 0;
        const char *mb_cmd = NULL;
        const char *mb_loader = NULL;
        mb_extract(&mb_total, &mb_cmd, &mb_loader);
        if (total_kb == 0 && mb_total) total_kb = mb_total;
        if ((!cpu || cpu[0] == '\0')) {
            if (mb_cmd && *mb_cmd) cpu = mb_cmd;
            else if (mb_loader && *mb_loader) cpu = mb_loader;
        }
    }

    /* right-side compact info block, but only show available fields */
    int info_col = 40;
    int row = 3;
    vga_write_at("SYS:", row, info_col, ATTR_YELLOW_ON_BLUE); row++;

    if (total_kb) {
        char tmp[16];
        u32_to_dec(total_kb, tmp, sizeof(tmp));
        vga_write_at("RAM tot(KB):", row, info_col, ATTR_WHITE_ON_BLUE);
        vga_write_at(tmp, row, info_col + 13, ATTR_WHITE_ON_BLUE);
        row++;
    }
    if (free_kb) {
        char tmp[16];
        u32_to_dec(free_kb, tmp, sizeof(tmp));
        vga_write_at("RAM free(KB):", row, info_col, ATTR_WHITE_ON_BLUE);
        vga_write_at(tmp, row, info_col + 13, ATTR_WHITE_ON_BLUE);
        row++;
    }
    if (storage_total) {
        char tmp[16];
        u32_to_dec(storage_total, tmp, sizeof(tmp));
        vga_write_at("STO tot(KB):", row, info_col, ATTR_WHITE_ON_BLUE);
        vga_write_at(tmp, row, info_col + 13, ATTR_WHITE_ON_BLUE);
        row++;
    }
    if (storage_free) {
        char tmp[16];
        u32_to_dec(storage_free, tmp, sizeof(tmp));
        vga_write_at("STO free(KB):", row, info_col, ATTR_WHITE_ON_BLUE);
        vga_write_at(tmp, row, info_col + 13, ATTR_WHITE_ON_BLUE);
        row++;
    }
    if (cpu && cpu[0]) {
        /* truncate if too long */
        int clen = (int)strlen_s(cpu);
        int max = 28;
        if (clen > max) {
            char buf[32];
            int i;
            for (i = 0; i < max && cpu[i]; ++i) buf[i] = cpu[i];
            buf[i] = '\0';
            vga_write_at("CPU:", row, info_col, ATTR_WHITE_ON_BLUE);
            vga_write_at(buf, row, info_col + 5, ATTR_WHITE_ON_BLUE);
        } else {
            vga_write_at("CPU:", row, info_col, ATTR_WHITE_ON_BLUE);
            vga_write_at(cpu, row, info_col + 5, ATTR_WHITE_ON_BLUE);
        }
        row++;
    }
    if (uptime) {
        char tmp[16];
        u32_to_dec(uptime, tmp, sizeof(tmp));
        vga_write_at("UPTIME(s):", row, info_col, ATTR_WHITE_ON_BLUE);
        vga_write_at(tmp, row, info_col + 11, ATTR_WHITE_ON_BLUE);
        row++;
    }

    /* bars (compact): place them below existing info if possible */
    int bar_row = row + 1;
    if (total_kb && free_kb && total_kb >= free_kb && bar_row + 2 < VGA_H - 3) {
        uint32_t used = (total_kb > free_kb) ? (total_kb - free_kb) : 0;
        uint32_t pct = safe_percent(used, total_kb);
        vga_write_at("RAM:", bar_row, info_col, ATTR_WHITE_ON_BLUE);
        draw_bar(bar_row + 1, info_col, 20, pct, ATTR_YELLOW_ON_BLUE, ATTR_WHITE_ON_BLUE);
        char ptxt[6]; u32_to_dec(pct, ptxt, sizeof(ptxt));
        vga_write_at(ptxt, bar_row + 1, info_col + 21, ATTR_WHITE_ON_BLUE);
        vga_write_at("%", bar_row + 1, info_col + 23, ATTR_WHITE_ON_BLUE);
        bar_row += 3;
    }
    if (storage_total && storage_free && storage_total >= storage_free && bar_row + 2 < VGA_H - 3) {
        uint32_t used = (storage_total > storage_free) ? (storage_total - storage_free) : 0;
        uint32_t pct = safe_percent(used, storage_total);
        vga_write_at("STO:", bar_row, info_col, ATTR_WHITE_ON_BLUE);
        draw_bar(bar_row + 1, info_col, 20, pct, ATTR_YELLOW_ON_BLUE, ATTR_WHITE_ON_BLUE);
        char ptxt[6]; u32_to_dec(pct, ptxt, sizeof(ptxt));
        vga_write_at(ptxt, bar_row + 1, info_col + 21, ATTR_WHITE_ON_BLUE);
        vga_write_at("%", bar_row + 1, info_col + 23, ATTR_WHITE_ON_BLUE);
        bar_row += 3;
    }

    /* footer / hints */
    vga_write_at("[M] Reboot", VGA_H - 2, VGA_W - 11, ATTR_YELLOW_ON_BLUE);
    vga_write_center("Website: https://chrysalisos.netlify.app/", VGA_H - 1, ATTR_YELLOW_ON_BLUE);

    /* serial mirror (only prints message + small sys summary) */
    serial_write("\n*** CHRYSALIS KERNEL PANIC ***\n");
    if (message && *message) serial_write(message);
    serial_write("\n");
}

/* --- public entry points --- */
extern "C" void panic(const char *message) {
    panic_ex(message, 0, 0, 0);
}
extern "C" void panic_ex(const char *message, uint32_t eip, uint32_t cs, uint32_t eflags) {
    asm volatile("cli");
    panic_render(message, eip, cs, eflags);

    for (;;) {
        uint8_t st = inb(0x64);
        if (st & 0x01) {
            uint8_t sc = inb(0x60);
            if (sc == SC_M_PRESS) try_reboot();
        }
        for (volatile int i = 0; i < 10000; ++i) { asm volatile(""); }
    }
}
