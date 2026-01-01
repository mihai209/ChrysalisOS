#pragma once
#include <stdint.h>
#include "isr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*irq_handler_t)(registers_t*);

void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

/* apelată din ASM */
void irq_handler(registers_t* regs);

#ifdef __cplusplus
}
#endif
