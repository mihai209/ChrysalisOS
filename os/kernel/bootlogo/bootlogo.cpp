#include "bootlogo.h"
#include "../terminal.h"
#include <stdint.h>

/* ===============================
   utilitare mici
================================ */

static void print(const char* s) {
    terminal_writestring(s);
}

static void print_hex(uint32_t v) {
    const char* hex = "0123456789ABCDEF";
    char buf[11] = "0x00000000";

    for (int i = 0; i < 8; i++) {
        buf[9 - i] = hex[v & 0xF];
        v >>= 4;
    }

    terminal_writestring(buf);
}

static void print_dec(uint32_t v) {
    char buf[16];
    int i = 0;

    if (v == 0) {
        terminal_putchar('0');
        return;
    }

    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }

    for (int j = i - 1; j >= 0; j--)
        terminal_putchar(buf[j]);
}

/* ===============================
   CPUID
================================ */

static inline void cpuid(
    uint32_t code,
    uint32_t* a,
    uint32_t* b,
    uint32_t* c,
    uint32_t* d
) {
    asm volatile("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(code)
    );
}

/* ===============================
   BOOT LOGO
================================ */

void bootlogo_show() {
    terminal_clear();

    print("=================================\n");
    print("     CHRYSALIS OS - BOOT INFO     \n");
    print("=================================\n\n");

    uint32_t a, b, c, d;

    // CPU vendor
    cpuid(0, &a, &b, &c, &d);

    char vendor[13];
    ((uint32_t*)vendor)[0] = b;
    ((uint32_t*)vendor)[1] = d;
    ((uint32_t*)vendor)[2] = c;
    vendor[12] = 0;

    print("CPU vendor     : ");
    print(vendor);
    print("\n");

    // CPU details
    cpuid(1, &a, &b, &c, &d);

    uint8_t stepping = a & 0xF;
    uint8_t model = (a >> 4) & 0xF;
    uint8_t family = (a >> 8) & 0xF;

    print("CPU family     : ");
    print_dec(family);
    print("\n");

    print("CPU model      : ");
    print_dec(model);
    print("\n");

    print("CPU stepping   : ");
    print_dec(stepping);
    print("\n");

    print("\nFeatures       : ");

    if (d & (1 << 0))  print("FPU ");
    if (d & (1 << 23)) print("MMX ");
    if (d & (1 << 25)) print("SSE ");
    if (d & (1 << 26)) print("SSE2 ");

    print("\n\n");

    print("Boot mode      : Protected 32-bit\n");
    print("Platform       : PC compatible\n");

    print("\n---------------------------------\n");
    print("Starting kernel...\n\n");
}
