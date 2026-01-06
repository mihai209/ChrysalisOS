#include "../fs/fat/fat.h"
#include "../terminal.h"
#include "../string.h"

extern "C" void cmd_fat(const char* args) {
    if (!args || !*args) {
        terminal_writestring("usage: fat <ls>\n");
        return;
    }

    if (strcmp(args, "ls") == 0) {
        fat32_list_root();
    } else if (strcmp(args, "info") == 0) {
        // Info este afișat la init, dar putem lista root ca info
        fat32_list_root();
    } else {
        terminal_writestring("unknown fat command. try 'fat ls'\n");
    }
}