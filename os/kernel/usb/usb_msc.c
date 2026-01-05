#include "usb_msc.h"
#include "../drivers/serial.h"

void usb_msc_init(uint8_t addr, uint8_t* config_desc, uint16_t config_len) {
    (void)config_desc;
    (void)config_len;
    
    serial_printf("[USB MSC] Mass Storage device detected at address %d. Bulk-Only/SCSI not yet implemented.\n", addr);
    // TODO: Find Bulk-In/Out endpoints, issue SCSI INQUIRY command.
}