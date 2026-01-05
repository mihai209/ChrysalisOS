#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void usb_msc_init(uint8_t addr, uint8_t* config_desc, uint16_t config_len);

#ifdef __cplusplus
}
#endif