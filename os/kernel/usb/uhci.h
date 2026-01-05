#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the UHCI controller with the given I/O base and IRQ */
void uhci_init(uint32_t io_base, uint8_t irq);

/* Performs a synchronous Control Transfer */
int uhci_control_transfer(uint8_t addr, uint8_t endp, void* setup, void* data, uint16_t len);

/* Sets up a polling interrupt transfer. Returns a handle (pointer to TD) */
void* uhci_setup_interrupt(uint8_t addr, uint8_t endp, void* data, uint16_t len);

/* Checks if an interrupt TD is active. If not, returns 1 (data ready) and reactivates it. */
int uhci_poll_interrupt(void* td_handle);

#ifdef __cplusplus
}
#endif