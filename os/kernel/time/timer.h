// kernel/time/timer.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void timer_init(uint32_t frequency);
uint64_t timer_ticks(void);
void sleep(uint32_t ms);

#ifdef __cplusplus
}
#endif
