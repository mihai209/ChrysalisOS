#include "../fs/fs.h"
#include "../terminal.h"
#include "../fs/chrysfs/chrysfs.h"
#include "../string.h"
#include "../mem/kmalloc.h"

extern "C" void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("usage: cat <file>\n");
        return;
    }
    const char* path = argv[1];

    /* Try ChrysFS first */
    if (strncmp(path, "/root", 5) == 0) {
        char *buf = (char*)kmalloc(4096);
        if (buf) {
            int bytes = chrysfs_read_file(path, buf, 4095);
            if (bytes >= 0) {
                buf[bytes] = 0;
                terminal_writestring(buf);
                terminal_writestring("\n");
                kfree(buf);
                return;
            }
            kfree(buf);
        }
    }

    /* Fallback to RAMFS */
    const FSNode* node = fs_find(path);
    if (!node) {
        terminal_writestring("file not found\n");
        return;
    }

    terminal_writestring(node->data);
}
