#include "usb_hub.h"
#include "../drivers/serial.h"

void usb_hub_init(uint8_t addr, uint8_t* config_desc, uint16_t config_len) {
    (void)config_desc;
    (void)config_len;
    
    serial_printf("[USB HUB] Hub detected at address %d. Port power/reset not yet implemented.\n", addr);
    // TODO: Get Hub Descriptor, power on ports, and listen for port status changes.
}