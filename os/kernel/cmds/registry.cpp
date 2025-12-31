#include "command.h"

extern "C" {

void cmd_clear(const char*);
void cmd_reboot(const char*);
void cmd_shutdown(const char*);
void cmd_ls(const char*);
void cmd_cat(const char*);
void cmd_touch(const char*);

Command command_table[] = {
    { "clear",    cmd_clear },
    { "reboot",   cmd_reboot },
    { "shutdown", cmd_shutdown },
    { "ls",       cmd_ls },
    { "cat",      cmd_cat },
    { "touch",    cmd_touch },
};

int command_count = sizeof(command_table) / sizeof(Command);

}
