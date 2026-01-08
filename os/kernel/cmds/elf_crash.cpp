// kernel/cmds/elf_crash.cpp
// Command: elf_crash (or elf_exec_crash) -> triggers panic for fun/testing

#include "elf_crash.h"
#include "../panic.h"
#include "../terminal.h"

extern "C" int cmd_elf_crash(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_printf("[elf_crash] invoking panic for fun (requested)\n");
    panic("elf_exec_crash requested via shell");
    /* panic is noreturn */
    return 0;
}

/* TODO: register cmd_elf_crash with your shell, e.g.
   shell_register("elf_crash", cmd_elf_crash);
*/
