#pragma once
#include <stdint.h>
#include "../interrupts/isr.h"

#ifdef __cplusplus
extern "C" {
#endif

void mouse_init(void);
void mouse_handler(registers_t* regs);

/* Compositor Synchronization */
void mouse_blit_start(void);
void mouse_blit_end(void);

#ifdef __cplusplus
}
#endif
