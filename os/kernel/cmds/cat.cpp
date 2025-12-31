#include "../fs/fs.h"
#include "../terminal.h"

extern "C" void cmd_cat(const char* args) {
    if (!args || !*args) {
        terminal_writestring("usage: cat <file>\n");
        return;
    }

    const FSNode* node = fs_find(args);
    if (!node) {
        terminal_writestring("file not found\n");
        return;
    }

    terminal_writestring(node->data);
}
