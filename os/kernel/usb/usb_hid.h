#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize a new HID device */
void usb_hid_init(uint8_t addr, uint8_t* config_desc, uint16_t config_len);

/* Poll all registered HID devices */
void usb_hid_poll(void);

#ifdef __cplusplus
}
#endif