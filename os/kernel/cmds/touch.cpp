#include "../fs/fs.h"

extern "C" void cmd_touch(const char* args) {
    if (!args || !*args)
        return;

    fs_create(args, "");
}
