#include "terminal.h"
#include "drivers/pic.h"
#include "arch/i386/gdt.h"
#include "arch/i386/idt.h"
#include "shell/shell.h"
#include "fs/fs.h"
#include "bootlogo/bootlogo.h"


extern "C" void kernel_main() {
    gdt_init();
    idt_init();
    pic_remap();

    terminal_init();
    bootlogo_show();
    shell_init();
    fs_init();
    terminal_writestring("Chrysalis OS\n");
    terminal_writestring("Type commands below:\n> ");

    asm volatile("sti");

    while (1) {
        asm volatile("hlt");
    }
}
