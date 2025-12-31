#include "../fs/fs.h"
#include "../terminal.h"

extern "C" void cmd_ls(const char*) {
    fs_list();
}
