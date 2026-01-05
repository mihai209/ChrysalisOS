#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void rdmsr(uint32_t msr, uint32_t *lo, uint32_t *hi) {
    asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

static inline void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi) {
    asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

static inline int cpu_has_msr(void) {
    uint32_t a, d;
    asm volatile("cpuid" : "=a"(a), "=d"(d) : "a"(1) : "ebx", "ecx");
    return (d & (1 << 5)) != 0;
}

#ifdef __cplusplus
}
#endif