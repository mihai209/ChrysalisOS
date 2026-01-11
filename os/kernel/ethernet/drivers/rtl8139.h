#pragma once

#define RTL_IDR0    0x00
#define RTL_RBSTART 0x30
#define RTL_CMD     0x37
#define RTL_IMR     0x3C
#define RTL_ISR     0x3E
#define RTL_RCR     0x44

int rtl8139_init(void);