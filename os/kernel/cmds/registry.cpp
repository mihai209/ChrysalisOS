#include "registry.h"
#include "clear.h"
#include "reboot.h"
#include "shutdown.h"
#include "ls.h"
#include "cat.h"
#include "touch.h"
#include "date.h"
#include "beep.h"
#include "play.h"

Command command_table[] = {
    { "beep",     cmd_beep},
    { "play",     cmd_play},
    { "clear",    cmd_clear },
    { "date",     cmd_date},
    { "reboot",   cmd_reboot },
    { "shutdown", cmd_shutdown },
    { "ls",       cmd_ls },
    { "cat",      cmd_cat },
    { "touch",    cmd_touch }
};

int command_count = sizeof(command_table) / sizeof(Command);
