#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Choose mode at compile time:
 * - define SCHED_COOPERATIVE to force cooperative mode (fallback).
 * - otherwise scheduler uses preemptive RR and expects PIT ticks.
 */
#ifndef SCHED_COOPERATIVE
/* default: preemptive */
#endif

void scheduler_init(uint32_t quantum_ticks);
void scheduler_tick(void);        /* call from PIT IRQ handler (increment tick) */
void scheduler_yield(void);       /* cooperative yield (or forced by tick) */
void scheduler_start(void);       /* start scheduling loop (call from kernel_main) */

#ifdef __cplusplus
}
#endif
