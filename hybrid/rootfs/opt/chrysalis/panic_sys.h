#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registration API - call these from your kernel startup when you know the values.
   They are safe, freestanding and tiny. */

void panic_sys_register_memory_kb(uint32_t total_kb, uint32_t free_kb);
void panic_sys_register_storage_kb(uint32_t total_kb, uint32_t free_kb);
void panic_sys_register_cpu_str(const char *cpu_str); /* pointer must be valid for lifetime */
void panic_sys_register_uptime_seconds(uint32_t uptime_seconds);

/* Query API used by panic.cpp */
uint32_t panic_sys_total_ram_kb(void);
uint32_t panic_sys_free_ram_kb(void);
uint32_t panic_sys_storage_total_kb(void);
uint32_t panic_sys_storage_free_kb(void);
const char* panic_sys_cpu_str(void); /* always returns a valid NUL-terminated string (fallback = "unknown") */
uint32_t panic_sys_uptime_seconds(void);

#ifdef __cplusplus
}
#endif
