#include "vfs.h"

#include "../terminal.h"
#include "../string.h"

#include "../fs/vfs/mount.h"
#include "../fs/vfs/vnode.h"

void cmd_vfs(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    terminal_writestring("VFS test:\n");

    vnode_t* root = vfs_resolve("/");

    if (!root) {
        terminal_writestring("  root = NULL (not mounted)\n");
        return;
    }

    terminal_writestring("  root mounted OK\n");

    terminal_writestring("  name: ");
    terminal_writestring(root->name ? root->name : "(null)");
    terminal_writestring("\n");

    terminal_writestring("  type: ");
    switch (root->type) {
        case VNODE_DIR:
            terminal_writestring("DIR\n");
            break;
        case VNODE_FILE:
            terminal_writestring("FILE\n");
            break;
        case VNODE_DEV:
            terminal_writestring("DEV\n");
            break;
        default:
            terminal_writestring("UNKNOWN\n");
            break;
    }

    terminal_writestring("  ops: ");
    if (root->ops)
        terminal_writestring("present\n");
    else
        terminal_writestring("NULL\n");
}
