#include "mount.h"
#include "../../string.h"

#define MAX_MOUNTS 8

static mount_t mounts[MAX_MOUNTS];
static int mount_count = 0;

void vfs_mount(const char* path, vnode_t* root)
{
    if (mount_count >= MAX_MOUNTS)
        return;

    mounts[mount_count].path = path;
    mounts[mount_count].root = root;
    mount_count++;
}
