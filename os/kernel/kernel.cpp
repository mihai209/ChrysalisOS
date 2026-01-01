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

/* --- DEBUG: activează asta pentru test rapid VGA --- */
/* Dacă vrei să rulezi testul VGA și apoi să blochezi, lasă VGA_TEST definit. */
/* După verificare, elimină/dezactivează acest define pentru a reveni la fluxul normal. */
#define VGA_TEST 1

extern "C" void kernel_main() {

#if VGA_TEST
    /* Initialize our VGA driver (does NOT call BIOS). 
       If you have a framebuffer from GRUB / multiboot, call vga_set_framebuffer() 
       before vga_init() from your boot code. */
    vga_init();
    vga_clear(0); // culoare 0 = negru

    /* desenăm o diagonală de test (culoare 15 = alb / 32bpp grayscale) */
    for (int i = 0; i < 200 && i < 320; i++) {
        vga_putpixel(i, i, 15);
    }

    /* desenăm și un chenar simplu pentru a confirma desenul pe ecran */
    for (int x = 10; x < 310; x++) {
        vga_putpixel(x, 10, 14);
        vga_putpixel(x, 190, 14);
    }
    for (int y = 10; y < 190; y++) {
        vga_putpixel(10, y, 14);
        vga_putpixel(310, y, 14);
    }

    /* rămânem blocați aici pentru a putea verifica vizual rezultatul în QEMU */
    for (;;)
        asm volatile("hlt");

#else
    /* Revenire la inițializările normale (originale) */
    // load_begin("GDT");
    gdt_init();
    // load_ok();

    // load_begin("IDT");
    idt_init();
    // load_ok();

    // load_begin("PIC");
    pic_remap();
    // load_ok();

    // load_begin("Terminal");
    terminal_init();
    // load_ok();

    // load_begin("Boot logo");
    bootlogo_show();
    // load_ok();

    // load_begin("Filesystem");
    fs_init();
    // load_ok();

    // load_begin("Shell");
    shell_init();
    // load_ok();

    // init keyboard buffer BEFORE keyboard init
    kbd_buffer_init();

    // load_begin("Keyboard IRQ");
    keyboard_init();
    // load_ok();

    // load_begin("PIT");
    pit_init(100);
    // load_ok();

    audio_init();
    mouse_init();

    // serial debug (optional)
    serial_init();
    serial_write_string("=== Chrysalis OS serial online ===\r\n");

    asm volatile("sti");

    terminal_writestring("\nSystem ready.\n> ");

    rtc_print();

    time_init();
    time_set_timezone(2);

    datetime t;
    time_get_local(&t);

    /* main loop: procesăm input din buffer, apoi idle */
    while (1) {
        shell_poll_input();
        asm volatile("hlt");
    }
#endif
}
