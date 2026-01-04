#include "../terminal.h"

extern "C" void cmd_buildinfo(const char* args)
{
    (void)args;

    terminal_writestring("Build information:\n");
    terminal_writestring(" Compiler: GCC\n");
    terminal_writestring(" Date: ");
    terminal_writestring(__DATE__);
    terminal_writestring("\n Time: ");
    terminal_writestring(__TIME__);
    terminal_writestring("\n Arch: i386\n");
}
