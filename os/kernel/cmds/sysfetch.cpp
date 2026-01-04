#include "../terminal.h"
#include "../drivers/pit.h"

extern "C" void cmd_sysfetch(const char* args)
{
    (void)args;

    terminal_writestring(" OS: Chrysalis OS\n");
    terminal_writestring(" Kernel: Custom x86\n");
    terminal_writestring(" Arch: i386\n");

    terminal_writestring(" Uptime (ticks): ");
    terminal_printf("%u\n", pit_get_ticks());
}
