#include "terminal.h"
#include "drivers/pic.h"
#include "arch/i386/gdt.h"
#include "arch/i386/idt.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "shell/shell.h"
#include "fs/fs.h"
#include "bootlogo/bootlogo.h"

extern "C" void kernel_main() {
    gdt_init();
    idt_init();
    pic_remap();

    terminal_init();
    bootlogo_show();

    fs_init();
    shell_init();

    keyboard_init();   // IRQ1
    pit_init(100);     // IRQ0

    asm volatile("sti");

    terminal_writestring("Chrysalis OS\n");
    terminal_writestring("Type commands below:\n> ");

    while (1) {
        asm volatile("hlt");
    }
}

