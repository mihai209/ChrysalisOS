#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal panic API */
void panic(const char *message);

/* Extended panic that kernel exception handlers can call if they have regs */
void panic_ex(const char *message, uint32_t eip, uint32_t cs, uint32_t eflags);

#ifdef __cplusplus
}
#endif
