#include "../fs/fs.h"
#include "../terminal.h"
#include "../fs/chrysfs/chrysfs.h"
#include "../string.h"

extern "C" void cmd_ls(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";

    /* Check if path targets the new AHCI mount */
    if (strcmp(path, "/") == 0 || strncmp(path, "/root", 5) == 0) {
        terminal_writestring("Listing (ChrysFS):\n");
        chrysfs_ls(path);
    } 
    /* Fallback to old RAMFS if path is not /root (or if we want to list ramfs specifically) */
    else if (strcmp(path, "ramfs") == 0) {
        terminal_writestring("Listing (RAMFS):\n");
        fs_list();
    } else {
        terminal_writestring("Unknown path. Try '/' or '/root/files'\n");
    }
}
