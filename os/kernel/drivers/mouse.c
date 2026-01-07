/* kernel/drivers/mouse.c
 * Software Mouse Cursor Subsystem for Chrysalis OS
 * Implements PS/2 Mouse Driver + Software Cursor Rendering + Scroll Wheel
 */

#include "mouse.h"
#include <stdbool.h>
#include "../arch/i386/io.h"
#include "../drivers/serial.h"
#include "../interrupts/irq.h"
#include "../video/fb_console.h"

/* Import serial logging from kernel glue */
extern void serial(const char *fmt, ...);

/* PS/2 Ports */
#define MOUSE_PORT_DATA    0x60
#define MOUSE_PORT_STATUS  0x64
#define MOUSE_PORT_CMD     0x64

/* Mouse State */
static uint8_t mouse_cycle = 0;
static int8_t  mouse_byte[4];
static bool    mouse_has_wheel = false;

/* Accumulator for Y movement to smooth scrolling */
static int32_t y_acc = 0;

/* --- PS/2 Helpers --- */

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) { // Wait for data (read)
        while (timeout--) {
            if ((inb(MOUSE_PORT_STATUS) & 1) == 1) return;
        }
    } else { // Wait for signal (write)
        while (timeout--) {
            if ((inb(MOUSE_PORT_STATUS) & 2) == 0) return;
        }
    }
}

static void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(MOUSE_PORT_CMD, 0xD4);
    mouse_wait(1);
    outb(MOUSE_PORT_DATA, write);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(MOUSE_PORT_DATA);
}

/* --- Interrupt Handler --- */

void mouse_handler(registers_t* regs) {
    (void)regs;
    uint8_t status = inb(MOUSE_PORT_STATUS);

    // Check if buffer has data and it is from mouse (Aux bit 5)
    if (!(status & 0x20)) return;

    uint8_t b = inb(MOUSE_PORT_DATA);

    if (mouse_cycle == 0) {
        // Byte 0 validation: Bit 3 must be 1
        if ((b & 0x08) == 0) return; 
        mouse_byte[0] = b;
        mouse_cycle++;
    } else if (mouse_cycle == 1) {
        mouse_byte[1] = b;
        mouse_cycle++;
    } else if (mouse_cycle == 2) {
        mouse_byte[2] = b;
        
        if (mouse_has_wheel) {
            mouse_cycle++; // Wait for 4th byte
        } else {
            goto process_packet;
        }
    } else if (mouse_cycle == 3) {
        mouse_byte[3] = b;
        goto process_packet;
    }
    return;

process_packet:
    mouse_cycle = 0;

    // Decode Packet
    // Byte 0: Yovfl, Xovfl, Ysign, Xsign, 1, Mid, Right, Left
    // Byte 1: X movement
    // Byte 2: Y movement
    // Byte 3: Z movement (Scroll)
    
    // int32_t dx = (int32_t)mouse_byte[1]; // Ignored for scroll-only
    int32_t dy = (int32_t)mouse_byte[2];

    // Handle signs (9-bit signed integers)
    if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;

    // Handle Scroll
    if (mouse_has_wheel) {
        int8_t dz = (int8_t)mouse_byte[3];
        if (dz != 0) {
            // Z > 0 = Scroll Up (Wheel forward)
            // Z < 0 = Scroll Down (Wheel backward)
            // Note: PS/2 Z direction might vary, usually +1 is up.
            
            /* Map wheel to console scroll. 
               Wheel backward (negative) -> Scroll down (advance text) -> positive lines 
               Wheel forward (positive) -> Scroll up (history) -> negative lines */
            serial("[MOUSE] Scroll wheel event: %d\n", dz);
            fb_cons_scroll(-dz);
        }
    }
    
    // Handle Y movement as scroll (fallback/alternative)
    if (dy != 0) {
        y_acc += dy;
        // Threshold for Y movement to trigger a scroll line (e.g., 10 units)
        int threshold = 10;
        if (y_acc > threshold || y_acc < -threshold) {
            int lines = y_acc / threshold;
            y_acc %= threshold;
            serial("[MOUSE] Vertical move scroll: %d lines\n", -lines);
            fb_cons_scroll(-lines); // dy > 0 is UP, so we scroll UP (negative lines)
        }
    }
}

/* --- Initialization --- */

void mouse_init(void) {
    serial("[MOUSE] subsystem initialized\n");

    uint8_t status;

    // 1. Enable Aux Device
    mouse_wait(1);
    outb(MOUSE_PORT_CMD, 0xA8);

    // 2. Enable IRQ12
    mouse_wait(1);
    outb(MOUSE_PORT_CMD, 0x20); // Read Config
    mouse_wait(0);
    status = inb(MOUSE_PORT_DATA) | 2;
    mouse_wait(1);
    outb(MOUSE_PORT_CMD, 0x60); // Write Config
    mouse_wait(1);
    outb(MOUSE_PORT_DATA, status);

    // 3. Set Defaults
    mouse_write(0xF6);
    mouse_read(); // ACK

    // 4. Enable Scroll Wheel (Intellimouse Magic Sequence)
    // Sequence: Set Rate 200, 100, 80
    mouse_write(0xF3); mouse_read(); mouse_write(200); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(100); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(80);  mouse_read();

    // Check ID
    mouse_write(0xF2);
    mouse_read(); // ACK
    uint8_t id = mouse_read();
    
    if (id == 3 || id == 4) {
        mouse_has_wheel = true;
        serial("[MOUSE] Scroll wheel detected (ID: %d)\n", id);
    } else {
        mouse_has_wheel = false;
        serial("[MOUSE] Standard PS/2 mouse (ID: %d)\n", id);
    }

    // 5. Enable Streaming
    mouse_write(0xF4);
    mouse_read(); // ACK

    // 6. Install Interrupt Handler
    irq_install_handler(12, mouse_handler);

    // 7. Unmask IRQ12 in PIC (Slave)
    // Master IRQ2 (Slave cascade) should be unmasked already
    uint8_t mask = inb(0xA1);
    outb(0xA1, mask & ~(1 << 4)); // Clear bit 4 (IRQ12 is IRQ4 on Slave)

    serial("[MOUSE] PS/2 mouse detected and enabled\n");
}