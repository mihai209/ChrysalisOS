#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the HPET subsystem: finds table, maps MMIO, enables counter */
void hpet_init(void);

/* Returns true if HPET is initialized and active */
bool hpet_is_active(void);

/* Returns the current main counter value (raw ticks) */
uint64_t hpet_get_ticks(void);

/* Global monotonic time functions (safe 64-bit math) */
uint64_t hpet_time_ns(void);
uint64_t hpet_time_us(void);
uint64_t hpet_time_ms(void);

/* Blocking delay using HPET */
void hpet_delay_us(uint32_t us);
void hpet_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif