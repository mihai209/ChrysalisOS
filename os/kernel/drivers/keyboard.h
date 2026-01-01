#pragma once
#include <stdint.h>
#include "../interrupts/isr.h"

#ifdef __cplusplus
extern "C" {
#endif

void keyboard_init();
void keyboard_handler(registers_t* regs);

#ifdef __cplusplus
}
#endif
