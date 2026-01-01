#include <stdint.h>
#include <stddef.h>

#include "terminal.h"
#include "drivers/pic.h"
#include "arch/i386/gdt.h"
#include "arch/i386/idt.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "shell/shell.h"
#include "fs/fs.h"
#include "bootlogo/bootlogo.h"
// #include "debug/load.h"   // dezactivat temporar
#include "drivers/serial.h"
#include "drivers/rtc.h"
#include "time/clock.h"
#include "drivers/audio/audio.h"
#include "drivers/mouse.h"
#include "input/keyboard_buffer.h" // pentru kbd_buffer_init()
#include "video/vga.h"

/* Dacă shell.h nu declară shell_poll_input(), avem o declarație locală ca fallback */
#ifdef __cplusplus
extern "C" {
#endif
void shell_poll_input(void);
#ifdef __cplusplus
}
#endif

/* Multiboot magic constant (classic Multiboot 1) */
#define MULTIBOOT_MAGIC 0x2BADB002u

/* Debug/test flag:
   - dacă rămâne definit, kernel va încerca testul grafic, DAR doar dacă framebufferul e prezent și suficient.
*/
#define VGA_TEST 1

/* Minimal multiboot info (only fields we read) */
typedef struct {
    uint32_t flags;
    /* ... many fields omitted ... */
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
} multiboot_info_t;

static void try_init_framebuffer_from_multiboot(uint32_t magic, uint32_t addr)
{
    if (magic != MULTIBOOT_MAGIC) return;
    if (addr == 0) return;

    multiboot_info_t* mbi = (multiboot_info_t*)(uintptr_t)addr;
    if (mbi->flags & (1 << 12)) {
        void* fb_addr = (void*)(uintptr_t)mbi->framebuffer_addr;
        uint32_t fb_width = mbi->framebuffer_width;
        uint32_t fb_height = mbi->framebuffer_height;
        uint32_t fb_pitch = mbi->framebuffer_pitch;
        uint8_t  fb_bpp = mbi->framebuffer_bpp;

        if (fb_addr && fb_width > 0 && fb_height > 0 && fb_pitch > 0 && fb_bpp > 0) {
            vga_set_framebuffer(fb_addr, fb_width, fb_height, fb_pitch, fb_bpp);
        }
    }
}

extern "C" void kernel_main(uint32_t magic, uint32_t addr) {

    /* try to pick up framebuffer info if GRUB provided it */
    try_init_framebuffer_from_multiboot(magic, addr);

#if VGA_TEST
    /* Initialize driver (will fall back to legacy 0xA0000 if no fb provided) */
    vga_init();

    /* Only run the visual graphics test if we actually have a framebuffer of at least 320x200.
       Otherwise continue with normal initialisation so you get the shell/terminal. */
    if (vga_has_framebuffer() && vga_get_width() >= 320 && vga_get_height() >= 200) {
        vga_clear(0);
        for (int i = 0; i < 200 && i < (int)vga_get_width(); i++) {
            vga_putpixel(i, i, 15);
        }
        for (int x = 10; x < 310 && x < (int)vga_get_width(); x++) {
            vga_putpixel(x, 10, 14);
            vga_putpixel(x, 190, 14);
        }
        for (int y = 10; y < 190 && y < (int)vga_get_height(); y++) {
            vga_putpixel(10, y, 14);
            vga_putpixel(310, y, 14);
        }

        /* stay here to inspect result */
        for (;;)
            asm volatile("hlt");
    } else {
        /* No usable framebuffer → fallback to normal boot path (text mode) */
        /* Continue to normal init below (no infinite-hlt) */
    }
#endif

    /* Normal init path (text mode, shell, etc) */
    gdt_init();
    idt_init();
    pic_remap();
    terminal_init();
    bootlogo_show();
    fs_init();
    shell_init();

    kbd_buffer_init();
    keyboard_init();
    pit_init(100);

    audio_init();
    mouse_init();

    serial_init();
    serial_write_string("=== Chrysalis OS serial online ===\r\n");

    asm volatile("sti");

    terminal_writestring("\nSystem ready.\n> ");

    rtc_print();

    time_init();
    time_set_timezone(2);

    datetime t;
    time_get_local(&t);

    while (1) {
        shell_poll_input();
        asm volatile("hlt");
    }
}
