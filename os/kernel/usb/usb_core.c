#include "usb_core.h"
#include "uhci.h"
#include "../drivers/serial.h"
#include "../arch/i386/io.h"
#include "../mem/kmalloc.h"
#include "../string.h"
#include "../input/keyboard_buffer.h"
#include "usb_hid.h"
#include "usb_hub.h"
#include "usb_msc.h"

/* PCI Configuration Port */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* Helper to read PCI config 32-bit */
static uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

/* Helper to read PCI config 16-bit */
static uint16_t pci_read_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

/* Helper to read PCI config 8-bit */
static uint8_t pci_read_config_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return (uint8_t)((inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF);
}

/* Global USB State */
static uint8_t usb_addr_counter = 1;

void usb_core_init(void) {
    serial_write_string("[USB] core initialized\r\n");

    /* Brute-force PCI scan for Intel PIIX3 UHCI (8086:7020) */
    /* We scan Bus 0, Slots 0-31, Function 0-7 */
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read_config_word(bus, slot, func, 0x00);
                if (vendor == 0xFFFF) continue;

                uint16_t device = pci_read_config_word(bus, slot, func, 0x02);

                /* Check for Intel PIIX3 UHCI */
                if (vendor == 0x8086 && device == 0x7020) {
                    serial_write_string("[USB] UHCI controller detected on PCI\r\n");

                    /* Read BAR4 (I/O Base) */
                    uint32_t bar4 = pci_read_config_dword(bus, slot, func, 0x20);
                    uint32_t io_base = bar4 & ~0x3; // Mask IO flag

                    /* Read IRQ Line (Offset 0x3C) */
                    uint8_t irq = pci_read_config_byte(bus, slot, func, 0x3C);

                    serial_printf("[USB] UHCI IO base = 0x%x\n", io_base);
                    serial_printf("[USB] UHCI IRQ = %d\n", irq);

                    /* Initialize UHCI driver */
                    uhci_init(io_base, irq);
                    
                    /* We found it, return (assuming single controller for now) */
                    return;
                }

                /* Check for multifunction device to continue scanning functions */
                if (func == 0) {
                    uint8_t header_type = pci_read_config_byte(bus, slot, func, 0x0E);
                    if (!(header_type & 0x80)) {
                        break; // Not multifunction, skip other functions
                    }
                }
            }
        }
    }
    
    serial_write_string("[USB] No UHCI controller found.\r\n");
}

/* --- Enumeration Logic --- */

void usb_attach_device(uint8_t port_id) {
    (void)port_id;
    serial_write_string("[USB] Starting enumeration...\r\n");

    /* 1. GET_DESCRIPTOR (DEVICE) - First 8 bytes to get MaxPacketSize0 */
    /* Actually prompt says 18 bytes. We'll ask for 18. */
    usb_setup_pkt_t setup;
    usb_dev_desc_t dev_desc;
    
    setup.bmRequestType = USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEVICE;
    setup.bRequest = USB_REQ_GET_DESCRIPTOR;
    setup.wValue = (USB_DESC_DEVICE << 8) | 0;
    setup.wIndex = 0;
    setup.wLength = 18;

    serial_write_string("[USB] GET_DESCRIPTOR (DEVICE)\r\n");
    if (uhci_control_transfer(0, 0, &setup, &dev_desc, 18) < 0) {
        serial_write_string("[USB] Failed to get device descriptor\r\n");
        return;
    }
    serial_write_string("[USB] device descriptor received\r\n");
    serial_printf("[USB] MaxPacketSize0 = %d\n", dev_desc.bMaxPacketSize0);
    serial_printf("[USB] Class: %x, SubClass: %x, Protocol: %x\n", 
                  dev_desc.bDeviceClass, dev_desc.bDeviceSubClass, dev_desc.bDeviceProtocol);
    serial_printf("[USB] VID: %x, PID: %x\n", dev_desc.idVendor, dev_desc.idProduct);


    /* 2. SET_ADDRESS */
    uint8_t new_addr = usb_addr_counter++;
    setup.bmRequestType = USB_RT_H2D | USB_RT_STANDARD | USB_RT_DEVICE;
    setup.bRequest = USB_REQ_SET_ADDRESS;
    setup.wValue = new_addr;
    setup.wIndex = 0;
    setup.wLength = 0;

    serial_printf("[USB] SET_ADDRESS = %d\n", new_addr);
    if (uhci_control_transfer(0, 0, &setup, 0, 0) < 0) {
        serial_write_string("[USB] Failed to set address\r\n");
        return;
    }
    serial_write_string("[USB] device address set\r\n");

    /* 3. GET_DESCRIPTOR (CONFIGURATION) - Full */
    /* Read 9 bytes first to get total length */
    uint8_t buf[256]; /* Buffer for config descriptor */
    
    setup.bmRequestType = USB_RT_D2H | USB_RT_STANDARD | USB_RT_DEVICE;
    setup.bRequest = USB_REQ_GET_DESCRIPTOR;
    setup.wValue = (USB_DESC_CONFIGURATION << 8) | 0;
    setup.wIndex = 0;
    setup.wLength = 9;

    serial_write_string("[USB] GET_DESCRIPTOR (CONFIG)\r\n");
    if (uhci_control_transfer(new_addr, 0, &setup, buf, 9) < 0) return;
    
    uint16_t total_len = *(uint16_t*)(buf + 2);
    serial_printf("[USB] configuration descriptor length = %d\n", total_len);

    if (total_len > 255 || total_len < 9) {
        serial_write_string("[USB] Invalid configuration descriptor length. Aborting.\r\n");
        return;
    }
    
    /* Read full config */
    setup.wLength = total_len;
    if (uhci_control_transfer(new_addr, 0, &setup, buf, total_len) < 0) return;

    /* Parse Config */
    /* We look for Interface Descriptor (Class 3 = HID) */
    uint8_t* ptr = buf;
    uint8_t* end = buf + total_len;
    
    /* Default to device class if interface class is 0 */
    uint8_t device_class = dev_desc.bDeviceClass;

    while (ptr < end) {
        uint8_t len = ptr[0];
        uint8_t type = ptr[1];
        if (len == 0) break; // Avoid infinite loop on malformed descriptor
        
        if (type == USB_DESC_INTERFACE) {
            serial_write_string("[USB] interface found\r\n");
            device_class = ptr[5]; // Use interface class
            break; // For now, we only care about the first interface
        }
        ptr += len;
    }

    /* Reset pointer to parse again for the specific driver */
    ptr = buf;

    /* Dispatch to class driver */
    switch (device_class) {
        case 0x03: /* HID */
            usb_hid_init(new_addr, ptr, total_len);
            break;
        case 0x08: /* Mass Storage */
            usb_msc_init(new_addr, ptr, total_len);
            break;
        case 0x09: /* Hub */
            usb_hub_init(new_addr, ptr, total_len);
            break;
        default:
            serial_printf("[USB] Unknown or unsupported device class: %x\n", device_class);
            break;
    }

    /* 4. SET_CONFIGURATION (Common for most devices) */
    setup.bmRequestType = USB_RT_H2D | USB_RT_STANDARD | USB_RT_DEVICE;
    setup.bRequest = USB_REQ_SET_CONFIGURATION;
    setup.wValue = 1; /* Config 1 */
    setup.wIndex = 0;
    setup.wLength = 0;

    serial_printf("[USB] SET_CONFIGURATION = 1\n");
    if (uhci_control_transfer(new_addr, 0, &setup, 0, 0) < 0) {
        serial_write_string("[USB] Failed to set configuration\r\n");
        return;
    }
    serial_write_string("[USB] device configured\r\n");
}

void usb_poll(void) {
    // This will now be handled by class drivers
    usb_hid_poll();
}