// kernel/cmds/elf_crash.cpp
// Command: elf_crash (or elf_exec_crash) -> triggers panic for fun/testing

#include "elf.h"   /* not strictly necessary but keeps cmd folder coherent */
#include <stdint.h>
#include <stddef.h>
#include "../terminal.h"
extern "C" void panic(const char* fmt, ...) __attribute__((noreturn));
extern void terminal_printf(const char* fmt, ...);

extern "C" int cmd_elf_crash(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_printf("[elf_crash] invoking panic for fun (requested)\n");
    /* call panic - adapt signature if your panic uses different name or args */
    panic("elf_exec_crash requested via shell");
    /* panic is noreturn; never returns */
    for (;;) {}
    return 0;
}

/* TODO: register cmd_elf_crash with your shell, e.g.
   shell_register("elf_crash", cmd_elf_crash);
*/
