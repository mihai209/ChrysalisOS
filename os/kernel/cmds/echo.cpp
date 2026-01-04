#include "../terminal.h"

extern "C" void cmd_echo(const char* args)
{
    if (!args || !*args) {
        terminal_writestring("\n");
        return;
    }

    terminal_writestring(args);
    terminal_writestring("\n");
}
