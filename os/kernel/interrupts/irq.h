#pragma once
#include <stdint.h>
#include "isr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*irq_handler_t)(registers_t*);

/* instalează / dezinstalează handler pentru un IRQ (0..15) */
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

/* apelată din ASM când vine un IRQ (vector 32..47) */
void irq_handler(registers_t* regs);

#ifdef __cplusplus
}
#endif
