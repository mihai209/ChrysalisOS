#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Main APIC Initialization */
void apic_init(void);

/* Check if APIC is active */
bool apic_is_active(void);

#ifdef __cplusplus
}
#endif