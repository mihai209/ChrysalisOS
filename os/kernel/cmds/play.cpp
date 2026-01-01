#include "../mlb/mlb.h"
#include "../terminal.h"
#include <stddef.h>

extern "C" void cmd_play(const char* args)
{
    if (!args || !args[0]) {
        terminal_writestring("Usage:\n");
        terminal_writestring("  play --catalog\n");
        terminal_writestring("  play <id>\n");
        terminal_writestring("  play --stop\n");
        return;
    }

    /* --catalog */
    if (args[0] == '-' && args[1] == '-' &&
        args[2] == 'c') {
        mlb_print_catalog();
        return;
    }

    /* --stop */
    if (args[0] == '-' && args[1] == '-' &&
        args[2] == 's') {
        mlb_stop();
        return;
    }

    /* altfel: considerăm că e ID-ul melodiei */
    mlb_play(args);
}
