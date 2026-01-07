#pragma once
#include <stdint.h>
#include "../interrupts/isr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inițializează controllerul PS/2 și tastatura */
void keyboard_init();

/* Handlerul de întrerupere (IRQ1) */
void keyboard_handler(registers_t* regs);

#ifdef __cplusplus
}
#endif
