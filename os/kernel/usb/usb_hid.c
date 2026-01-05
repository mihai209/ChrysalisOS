#include "usb_hid.h"
#include "usb_core.h"
#include "uhci.h"
#include "../drivers/serial.h"
#include "../mem/kmalloc.h"
#include "../string.h"
#include "../input/keyboard_buffer.h"

#define MAX_HID_DEVICES 4

typedef struct {
    int active;
    uint8_t addr;
    uint8_t ep;
    void* td_handle;
    uint8_t* buffer;
    uint8_t prev_buffer[8];
} hid_device_t;

static hid_device_t hid_devices[MAX_HID_DEVICES];

/* Simple HID to ASCII Map (US QWERTY) */
static const char hid_ascii_map[128] = {
    0, 0, 0, 0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
    'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\n', 0, '\b', '\t',
    ' ', '-', '=', '[', ']', '\\', 0, ';', '\'', '`', ',', '.', '/', 0, 0, 0
};

void usb_hid_init(uint8_t addr, uint8_t* config_desc, uint16_t config_len) {
    serial_write_string("[USB HID] Initializing device...\r\n");

    uint8_t* ptr = config_desc;
    uint8_t* end = config_desc + config_len;
    uint8_t hid_ep = 0;
    uint16_t hid_ep_size = 8;

    while (ptr < end) {
        uint8_t len = ptr[0];
        uint8_t type = ptr[1];
        if (len == 0) break;

        if (type == USB_DESC_INTERFACE) {
            if (ptr[5] == 0x03 && ptr[6] == 0x01 && ptr[7] == 0x01) {
                serial_write_string("[USB HID] Keyboard interface found\r\n");
            }
        } else if (type == USB_DESC_ENDPOINT) {
            if ((ptr[2] & 0x80) && (ptr[3] & 0x03) == 0x03) {
                hid_ep = ptr[2] & 0x0F;
                hid_ep_size = *(uint16_t*)(ptr + 4);
                serial_printf("[USB HID] Interrupt IN endpoint found: 0x8%x, size: %d\n", hid_ep, hid_ep_size);
                break;
            }
        }
        ptr += len;
    }

    if (hid_ep) {
        for (int i = 0; i < MAX_HID_DEVICES; i++) {
            if (!hid_devices[i].active) {
                hid_devices[i].active = 1;
                hid_devices[i].addr = addr;
                hid_devices[i].ep = hid_ep;
                hid_devices[i].buffer = (uint8_t*)kmalloc(hid_ep_size);
                hid_devices[i].td_handle = uhci_setup_interrupt(addr, hid_ep, hid_devices[i].buffer, hid_ep_size);
                memset(hid_devices[i].prev_buffer, 0, 8);
                serial_write_string("[USB HID] Polling started for new device.\r\n");
                return;
            }
        }
    }
}

void usb_hid_poll(void) {
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (!hid_devices[i].active) continue;

        hid_device_t* dev = &hid_devices[i];

        if (uhci_poll_interrupt(dev->td_handle)) {
            uint8_t modifiers = dev->buffer[0];
            int shift = (modifiers & 0x02) || (modifiers & 0x20);

            for (int k = 2; k < 8; k++) {
                uint8_t key = dev->buffer[k];
                if (key > 0) {
                    int new_press = 1;
                    for (int j = 2; j < 8; j++) {
                        if (dev->prev_buffer[j] == key) {
                            new_press = 0;
                            break;
                        }
                    }

                    if (new_press) {
                        // serial_printf("[USB HID] Keycode: %x\n", key); // Comentat pentru a reduce spam-ul
                        if (key < 128) {
                            char c = hid_ascii_map[key];
                            if (shift && c >= 'a' && c <= 'z') {
                                c -= 32;
                            }
                            if (c) {
                                kbd_push(c);
                            }
                        }
                    }
                }
            }
            memcpy(dev->prev_buffer, dev->buffer, 8);
        }
    }
}