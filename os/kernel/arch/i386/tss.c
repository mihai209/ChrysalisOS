#include "tss.h"
#include "gdt.h"
#include "../../terminal.h"
#include "../../panic.h"
//#include <string.h>

int tss_loaded = 0;

extern void tss_flush();

static struct tss_entry tss;

static void k_memset(void* dst, int v, uint32_t n) {
    uint8_t* p = (uint8_t*)dst;
    while (n--) *p++ = (uint8_t)v;
}


void tss_set_kernel_stack(uint32_t stack)
{
    tss.esp0 = stack;
}

void tss_init(uint32_t kernel_stack)
{
    k_memset(&tss, 0, sizeof(tss));

    tss.ss0  = 0x10;          // kernel data selector
    tss.esp0 = kernel_stack;

    tss.cs = 0x0B;            // user code | ring 3
    tss.ss = 0x13;            // user data | ring 3
    tss.ds = 0x13;
    tss.es = 0x13;
    tss.fs = 0x13;
    tss.gs = 0x13;

    tss.iomap_base = sizeof(tss);

    /* GDT entry pentru TSS */
    uint32_t base  = (uint32_t)&tss;
    uint32_t limit = sizeof(tss);

    gdt_set_gate(
        5,                    // index TSS (IMPORTANT!)
        base,
        limit,
        0x89,                 // present, ring 0, TSS
        0x40
    );

    tss_flush();

    tss_loaded = 1;

    terminal_writestring("[tss] loaded successfully\n");
}
