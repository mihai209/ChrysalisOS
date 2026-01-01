#include "../terminal.h"

extern "C" void cmd_example(const char* args)
{
    if (!args || !*args) {
        terminal_writestring("usage: example <text>\n");
        return;
    }

    terminal_writestring("you typed: ");
    terminal_writestring(args);
    terminal_writestring("\n");
}
