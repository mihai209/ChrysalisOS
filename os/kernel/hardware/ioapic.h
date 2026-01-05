#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize IOAPIC */
bool ioapic_init(uint32_t base_addr, uint32_t gsi_base);

/* Route a legacy IRQ (0-15) to a vector */
void ioapic_set_entry(uint8_t index, uint64_t data);

#ifdef __cplusplus
}
#endif