#include "rtl8139.h"
#include "../net_device.h"
#include "../../drivers/serial.h"

/* Import serial logging */
extern void serial(const char *fmt, ...);

/* Stub implementation for fallback logic */
int rtl8139_init(void) {
    /* Real implementation would scan PCI for 0x10EC:0x8139 */
    /* For now, we just return -1 to let E1000 take over or fail gracefully */
    /* serial("[RTL8139] Probing...\n"); */
    return -1; 
}