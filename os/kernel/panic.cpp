// kernel/panic.cpp
// Windows XP Style Blue Screen of Death (BSOD) pentru Chrysalis OS

#include "panic.h"
#include "panic_sys.h"
#include "arch/i386/tss.h"
// === Freestanding headers (fără libc) ===
typedef unsigned int       size_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;

// === Fallback strlen sigur ===
static size_t strlen(const char* str) {
    const char* s = str;
    while (*s) ++s;
    return s - str;
}

// === VGA Text Mode ===
#define VGA_MEM ((volatile uint16_t*)0xB8000)
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

// === Culori Windows XP BSOD originale ===
#define XP_BG      1   // Albastru intens (background)
#define XP_FG      15 // Alb (foreground)
#define XP_ATTR    ((XP_BG << 4) | (XP_FG & 0x0F))

// === I/O Ports ===
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

// === Serial debug (COM1) ===
static void serial_print(const char* s) {
    while (*s) {
        while (!(inb(0x3F8 + 5) & 0x20));
        outb(0x3F8, *s++);
    }
}

// === Primitivă de scriere caracter ===
static void put_char(int x, int y, char c) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT)
        VGA_MEM[y * VGA_WIDTH + x] = ((uint16_t)XP_ATTR << 8) | (uint8_t)c;
}

// === Scrie string centrat sau la poziție fixă ===
static void draw_string(int x, int y, const char* s) {
    while (*s) {
        put_char(x++, y, *s++);
    }
}

static void draw_string_center(int y, const char* s) {
    size_t len = strlen(s);
    int x = (VGA_WIDTH - (int)len) / 2;
    draw_string(x, y, s);
}

// === Umple tot ecranul cu fundal albastru + spații (autentic XP) ===
static void clear_screen_xp() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEM[i] = ((uint16_t)XP_ATTR << 8) | ' ';
    }
}

// === Convert u32 → string (fără diviziune libc) ===
static void u32_to_hex(uint32_t value, char* buf) {
    const char* hex = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[value & 0xF];
        value >>= 4;
    }
    buf[10] = 0;
}

static void u32_to_dec(uint32_t value, char* buf) {
    char temp[12];
    int i = 0;
    if (value == 0) {
        buf[0] = '0'; buf[1] = 0;
        return;
    }
    while (value) {
        temp[i++] = (value % 10) + '0';
        value /= 10;
    }
    int j = 0;
    while (i > 0) buf[j++] = temp[--i];
    buf[j] = 0;
}

extern "C" void panic_render_pretty(const char* msg) {
    clear_screen_xp();

    int line = 1;  // Pornim și mai sus pentru compactitate
    int indent = 4;  // Indentare mică la stânga (la început)

    draw_string(indent, line++, "A problem has been detected and Chrysalis has been");
    draw_string(indent, line++, "shut down to prevent damage to your computer.");
    line += 1;  // Spațiu mic

    draw_string(indent, line++, "KERNEL_SECURITY_CHECK_FAILURE");
    line += 1;

    // Mesaj custom dacă există, împărțit pe linii și aliniat la stânga
    if (msg && msg[0]) {
        char buffer[76];  // Maxim 76 char/linie pentru a nu ieși
        const char* p = msg;
        while (*p) {
            int i = 0;
            while (*p && *p != '\n' && i < 75) {
                buffer[i++] = *p++;
            }
            buffer[i] = 0;
            draw_string(indent, line++, buffer);
            if (*p == '\n') p++;
        }
        line += 1;
    } else {
        draw_string(indent, line++, "If this is the first time you've seen this error screen,");
        draw_string(indent, line++, "restart your computer.");
        line += 1;
    }


    if (!tss_loaded) {
    panic("Ring3 failed: TSS not loaded");
}


    // Instrucțiuni tehnice - aliniate la stânga, compact
    draw_string(indent, line++, "Check to make sure any new hardware or software is properly installed.");
    draw_string(indent, line++, "If problems continue, disable or remove newly installed hardware");
    draw_string(indent, line++, "or software. Disable BIOS memory options such as caching or shadowing.");
    line += 1;

    // Footer
    draw_string(indent, line++, "Press M for emergency reboot");
    draw_string(indent, line++, "Chrysalis OS - chrysalisos.netlify.app");

    // Protecție contra depășire (șterge jos dacă e cazul)
    for (int y = line; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            put_char(x, y, ' ');
        }
    }
}

// === Reboot forțat ===
extern "C" void try_reboot() {
    // Pulse pe keyboard controller pentru reset
    outb(0x64, 0xFE);
    // Fallback: triple fault
    for (volatile int i = 0; i < 1000000; i++);
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) idt = {0, 0};
    asm volatile("lidt %0; int $3" : : "m"(idt));
}

// === Handler panic principal ===
extern "C" void panic_ex(const char *msg, uint32_t eip, uint32_t cs, uint32_t eflags) {
    (void)eip; (void)cs; (void)eflags;
    asm volatile("cli");  // dezactivează interrupțiile

    serial_print("\n!!! KERNEL PANIC !!!\n");
    if (msg) {
        serial_print(msg);
    }
    serial_print("\nEIP: ");
    char hexbuf[12];
    u32_to_hex(eip, hexbuf);
    serial_print(hexbuf);
    serial_print("\n");

    panic_render_pretty(msg);

    // Loop infinit, așteaptă tasta M pentru reboot
    for (;;) {
        if (inb(0x64) & 1) {              // key ready
            uint8_t scancode = inb(0x60);
            if (scancode == 0x32) {      // M key make code
                try_reboot();
            }
        }
    }
}

extern "C" void panic(const char *msg) {
    panic_ex(msg, 0, 0, 0);
}