/* kernel/drivers/mouse.c
 * Software Mouse Cursor Subsystem for Chrysalis OS
 * Implements PS/2 Mouse Driver + Software Cursor Rendering + Scroll Wheel
 */

#include "mouse.h"
#include <stdbool.h>
#include "../arch/i386/io.h"
#include "../drivers/serial.h"
#include "../interrupts/irq.h"
#include "../video/gpu.h"
#include "../video/fb_console.h"
#include "../input/input.h"

/* Import serial logging from kernel glue */
extern void serial(const char *fmt, ...);

/* PS/2 Ports */
#define MOUSE_PORT_DATA    0x60
#define MOUSE_PORT_STATUS  0x64
#define MOUSE_PORT_CMD     0x64

/* Mouse State */
static uint8_t mouse_cycle = 0;
static int8_t  mouse_byte[4];
static int32_t mouse_x = 0;
static int32_t mouse_y = 0;
static int32_t prev_mouse_x = 0;
static int32_t prev_mouse_y = 0;
static bool    mouse_has_wheel = false;
static bool    cursor_drawn = false;
static uint8_t prev_buttons = 0;
static volatile bool mouse_blocked = false;

/* Cursor Bitmap (16x16)
 * 0 = Transparent
 * 1 = Black (Border)
 * 2 = White (Fill)
 */
static const uint8_t cursor_bitmap[16 * 16] = {
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,
    1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,
    1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,
    1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,
    1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,
    1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,
    1,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,
    1,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,
    1,2,2,1,2,2,1,0,0,0,0,0,0,0,0,0,
    1,2,1,0,1,2,2,1,0,0,0,0,0,0,0,0,
    1,1,0,0,1,2,2,1,0,0,0,0,0,0,0,0,
    1,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0
};

/* Save-under buffer to restore background */
static uint32_t cursor_save_buffer[16 * 16];

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

/* --- Software Cursor Rendering --- */

static uint32_t get_pixel(gpu_device_t* dev, int x, int y) {
    if (!dev || !dev->virt_addr) return 0;
    if (x < 0 || x >= (int)dev->width || y < 0 || y >= (int)dev->height) return 0;

    /* Assuming 32bpp for simplicity as per kernel setup */
    uint32_t offset = y * (dev->pitch / 4) + x;
    uint32_t* fb = (uint32_t*)dev->virt_addr;
    return fb[offset];
}

static void draw_pixel(gpu_device_t* dev, int x, int y, uint32_t color) {
    if (dev && dev->ops && dev->ops->putpixel) {
        dev->ops->putpixel(dev, x, y, color);
    }
}

static void restore_cursor_background(gpu_device_t* dev) {
    if (!cursor_drawn) return;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int px = prev_mouse_x + x;
            int py = prev_mouse_y + y;
            draw_pixel(dev, px, py, cursor_save_buffer[y * 16 + x]);
        }
    }
    cursor_drawn = false;
}

static void save_cursor_background(gpu_device_t* dev, int mx, int my) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int px = mx + x;
            int py = my + y;
            cursor_save_buffer[y * 16 + x] = get_pixel(dev, px, py);
        }
    }
}

static void draw_cursor_sprite(gpu_device_t* dev, int mx, int my) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            uint8_t type = cursor_bitmap[y * 16 + x];
            int px = mx + x;
            int py = my + y;

            if (type == 1) {
                draw_pixel(dev, px, py, 0xFF000000); // Black border
            } else if (type == 2) {
                draw_pixel(dev, px, py, 0xFFFFFFFF); // White fill
            }
        }
    }
}

static void update_cursor(void) {
    /* If blocked by compositor, do not draw. Compositor will call end() later. */
    if (mouse_blocked) return;

    gpu_device_t* gpu = gpu_get_primary();
    if (!gpu) return;

    restore_cursor_background(gpu);
    save_cursor_background(gpu, mouse_x, mouse_y);
    draw_cursor_sprite(gpu, mouse_x, mouse_y);

    prev_mouse_x = mouse_x;
    prev_mouse_y = mouse_y;
    cursor_drawn = true;
}

/* --- Compositor Sync API --- */

void mouse_blit_start(void) {
    asm volatile("cli");
    mouse_blocked = true;
    gpu_device_t* gpu = gpu_get_primary();
    /* Erase cursor from VRAM so compositor can draw clean frame */
    if (gpu && cursor_drawn) {
        restore_cursor_background(gpu);
    }
    asm volatile("sti");
}

void mouse_blit_end(void) {
    asm volatile("cli");
    mouse_blocked = false;
    /* Redraw cursor on top of new frame */
    update_cursor();
    asm volatile("sti");
}

/* --- Interrupt Handler --- */
void mouse_handler(registers_t* regs) {
    (void)regs;
    
    /* Loop to handle all pending mouse packets */
    int loops = 0;
    while (loops < 100) {
        uint8_t status = inb(MOUSE_PORT_STATUS);
        
        /* Check if buffer has data (Bit 0) and it is from mouse (Aux bit 5) */
        if (!((status & 0x01) && (status & 0x20))) break;

        uint8_t b = inb(MOUSE_PORT_DATA);

    if (mouse_cycle == 0) {
        /* Byte 0 validation: Bit 3 must be 1 */
        if ((b & 0x08) == 0) {
            /* Invalid byte, reset cycle and try next byte in buffer */
            mouse_cycle = 0;
            continue;
        }
        mouse_byte[0] = b;
        mouse_cycle++;
    } else if (mouse_cycle == 1) {
        mouse_byte[1] = b;
        mouse_cycle++;
    } else if (mouse_cycle == 2) {
        mouse_byte[2] = b;
        mouse_cycle++;
    } else if (mouse_cycle == 3) {
        mouse_byte[3] = b;
        mouse_cycle++;
    }
    else {
        loops++;
        continue; // Should not happen if logic is correct
    }
    
    if (mouse_cycle < (mouse_has_wheel ? 4 : 3)) {
        loops++;
        continue;
    }

    mouse_cycle = 0;

    /* Decode Packet */
    int32_t dx = (uint8_t)mouse_byte[1];
    int32_t dy = (uint8_t)mouse_byte[2];

    /* Handle signs (9-bit signed integers) */
    if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00;
    if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;

    /* Update Position */
    mouse_x += dx;
    mouse_y -= dy; /* PS/2 Y is bottom-to-top */

    /* Clamp to screen */
    gpu_device_t* gpu = gpu_get_primary();
    if (gpu) {
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= (int)gpu->width) mouse_x = gpu->width - 1;
        if (mouse_y >= (int)gpu->height) mouse_y = gpu->height - 1;
    }

    /* Handle Scroll */
    if (mouse_has_wheel) {
        int8_t dz = (int8_t)mouse_byte[3];
        if (dz != 0) {
            /* Map wheel to Button 4 (Up) and 5 (Down) events */
            input_event_t ev;
            ev.type = INPUT_MOUSE_CLICK;
            ev.mouse_x = mouse_x;
            ev.mouse_y = mouse_y;
            ev.pressed = true; /* Momentary press */
            ev.keycode = (dz > 0) ? 4 : 5; /* 4=Up, 5=Down */
            input_push(ev);
        }
    }

    /* Update Cursor on Screen */
    update_cursor();
    
    /* Push Input Events */
    if (dx != 0 || dy != 0) {
        input_event_t ev;
        ev.type = INPUT_MOUSE_MOVE;
        ev.mouse_x = mouse_x;
        ev.mouse_y = mouse_y;
        ev.pressed = false;
        ev.keycode = 0;
        input_push(ev);
    }

    uint8_t buttons = mouse_byte[0] & 0x07;

    /* Left Button */
    if ((buttons & 1) != (prev_buttons & 1)) {
        input_event_t ev;
        ev.type = INPUT_MOUSE_CLICK;
        ev.mouse_x = mouse_x;
        ev.mouse_y = mouse_y;
        ev.pressed = (buttons & 1) ? true : false;
        ev.keycode = 1; /* 1 = Left */
        input_push(ev);
    }
    /* Right Button */
    if ((buttons & 2) != (prev_buttons & 2)) {
        input_event_t ev;
        ev.type = INPUT_MOUSE_CLICK;
        ev.mouse_x = mouse_x;
        ev.mouse_y = mouse_y;
        ev.pressed = (buttons & 2) ? true : false;
        ev.keycode = 2; /* 2 = Right */
        input_push(ev);
    }

    prev_buttons = buttons;

    loops++;
    }
}

/* --- Initialization --- */

void mouse_init(void) {
    serial("[MOUSE] subsystem initialized\n");

    /* Note: Controller setup (IRQ12 enable, Port enable) is done in keyboard_init.
       We just reset the mouse device itself. */

    /* 1. Reset Mouse */
    mouse_write(0xF6);
    mouse_read(); /* ACK */

    /* 4. Enable Scroll Wheel (Intellimouse Magic Sequence) */
    /* Sequence: Set Rate 200, 100, 80 */
    mouse_write(0xF3); mouse_read(); mouse_write(200); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(100); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(80);  mouse_read();

    /* Check ID */
    mouse_write(0xF2);
    mouse_read(); /* ACK */
    uint8_t id = mouse_read();
    
    if (id == 3 || id == 4) {
        mouse_has_wheel = true;
        serial("[MOUSE] Scroll wheel detected (ID: %d)\n", id);
    } else {
        mouse_has_wheel = false;
        serial("[MOUSE] Standard PS/2 mouse (ID: %d)\n", id);
    }

    /* 2. Enable Streaming */
    mouse_write(0xF4);
    mouse_read(); /* ACK */

    /* 3. Install Interrupt Handler */
    irq_install_handler(12, mouse_handler);

    /* 4. Unmask IRQ12 in PIC (Slave) */
    uint8_t mask = inb(0xA1);
    outb(0xA1, mask & ~(1 << 4));

    serial("[MOUSE] PS/2 mouse detected and enabled\n");

    /* Center cursor initially */
    gpu_device_t* gpu = gpu_get_primary();
    if (gpu) {
        mouse_x = gpu->width / 2;
        mouse_y = gpu->height / 2;
        update_cursor();
    }
}