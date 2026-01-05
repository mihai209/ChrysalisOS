#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize Local APIC */
bool lapic_init(uint32_t base_addr);

/* Send End of Interrupt to LAPIC */
void lapic_eoi(void);

/* Get LAPIC ID */
uint32_t lapic_get_id(void);

/* Send an Inter-Processor Interrupt (IPI) */
void lapic_send_ipi(uint8_t apic_id, uint32_t type, uint8_t vector);

#ifdef __cplusplus
}
#endif