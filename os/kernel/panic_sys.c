// panic_sys.c
// Implements panic_sys_* API. CPUID-based CPU detection + registration storage.
// Build with gcc -m32.

#include "panic_sys.h"
#include <stdint.h>
#include <stddef.h>

/* Internal storage (strong, file-local) */
static volatile uint32_t g_total_ram_kb = 0;
static volatile uint32_t g_free_ram_kb = 0;
static volatile uint32_t g_storage_total_kb = 0;
static volatile uint32_t g_storage_free_kb = 0;
static const char* g_cpu_str = NULL;
static volatile uint32_t g_uptime_s = 0;

/* CPUID helper */
static inline void cpuid(uint32_t eax_in, uint32_t ecx_in,
                         uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    uint32_t a_, b_, c_, d_;
    asm volatile("cpuid"
                 : "=a"(a_), "=b"(b_), "=c"(c_), "=d"(d_)
                 : "a"(eax_in), "c"(ecx_in));
    if (a) *a = a_;
    if (b) *b = b_;
    if (c) *c = c_;
    if (d) *d = d_;
}

/* produce a static CPU brand string (max 64 bytes) */
static char cpu_brand_buf[64];
static void detect_cpu_brand_once(void)
{
    if (g_cpu_str) return; /* if registered externally, respect that */

    /* check highest extended function (0x80000000) */
    uint32_t a,b,c,d;
    cpuid(0x80000000u, 0, &a, &b, &c, &d);
    uint32_t max_ext = a;
    if (max_ext >= 0x80000004u) {
        /* three leaves 0x80000002..0x80000004 contain the brand string (48 bytes) */
        char *p = cpu_brand_buf;
        for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u; ++leaf) {
            cpuid(leaf, 0, &a, &b, &c, &d);
            /* copy registers a,b,c,d as bytes in little-endian order */
            *(uint32_t*)p = a; p += 4;
            *(uint32_t*)p = b; p += 4;
            *(uint32_t*)p = c; p += 4;
            *(uint32_t*)p = d; p += 4;
        }
        /* ensure NUL termination and trim leading/trailing spaces */
        cpu_brand_buf[48] = '\0';
        /* trim leading spaces */
        char *s = cpu_brand_buf;
        while (*s == ' ') ++s;
        /* move trimmed to start */
        if (s != cpu_brand_buf) {
            char tmp[64];
            size_t i = 0;
            while (s[i] && i < sizeof(tmp)-1) { tmp[i] = s[i]; ++i; }
            tmp[i] = '\0';
            /* copy back */
            for (size_t j=0;j<=i; ++j) cpu_brand_buf[j] = tmp[j];
        }
        g_cpu_str = cpu_brand_buf;
        return;
    }

    /* fallback: vendor string (cpuid leaf 0) */
    cpuid(0, 0, &a, &b, &c, &d);
    /* vendor in EBX, EDX, ECX */
    char vendor[13];
    ((uint32_t*)vendor)[0] = b;
    ((uint32_t*)vendor)[1] = d;
    ((uint32_t*)vendor)[2] = c;
    vendor[12] = '\0';
    /* format "CPU: vendor" */
    const char *fmt = "CPU: ";
    size_t i = 0;
    for (; fmt[i]; ++i) cpu_brand_buf[i] = fmt[i];
    size_t base = i;
    for (size_t j=0; j < 12 && vendor[j]; ++j) cpu_brand_buf[base + j] = vendor[j];
    cpu_brand_buf[base + 12] = '\0';
    g_cpu_str = cpu_brand_buf;
}

/* Registration functions - call these from kernel when you collect info */
void panic_sys_register_memory_kb(uint32_t total_kb, uint32_t free_kb) {
    g_total_ram_kb = total_kb;
    g_free_ram_kb = free_kb;
}
void panic_sys_register_storage_kb(uint32_t total_kb, uint32_t free_kb) {
    g_storage_total_kb = total_kb;
    g_storage_free_kb = free_kb;
}
void panic_sys_register_cpu_str(const char *cpu_str) {
    g_cpu_str = cpu_str;
}
void panic_sys_register_uptime_seconds(uint32_t uptime_seconds) {
    g_uptime_s = uptime_seconds;
}

/* Query functions */
uint32_t panic_sys_total_ram_kb(void) { return g_total_ram_kb; }
uint32_t panic_sys_free_ram_kb(void)  { return g_free_ram_kb; }
uint32_t panic_sys_storage_total_kb(void) { return g_storage_total_kb; }
uint32_t panic_sys_storage_free_kb(void)  { return g_storage_free_kb; }
uint32_t panic_sys_uptime_seconds(void) { return g_uptime_s; }

/* CPU string: ensure we detect if not registered */
const char* panic_sys_cpu_str(void) {
    if (g_cpu_str) return g_cpu_str;
    detect_cpu_brand_once();
    if (g_cpu_str) return g_cpu_str;
    return "unknown";
}
