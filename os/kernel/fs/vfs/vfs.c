#include "mount.h"
#include "vnode.h"
#include <stdint.h>

/* Use project string lib; if you don't have it replace with <string.h> */
#include "../../string.h"

#define MAX_MOUNTS 8

typedef struct mount {
    const char* path;   /* mount point string (e.g. "/") */
    vnode_t* root;      /* root vnode of the mounted FS */
} mount_t;

static mount_t mounts[MAX_MOUNTS];
static int mount_count = 0;

void vfs_mount(const char* path, vnode_t* root)
{
    if (!path || !root)
        return;

    /* simple duplicate check */
    for (int i = 0; i < mount_count; i++) {
        if (strcmp(mounts[i].path, path) == 0) {
            mounts[i].root = root; /* replace */
            return;
        }
    }

    if (mount_count >= MAX_MOUNTS)
        return;

    mounts[mount_count].path = path;
    mounts[mount_count].root = root;
    mount_count++;
}

vnode_t* vfs_resolve(const char* path)
{
    if (!path)
        return 0;

    /* only absolute paths supported for now */
    if (path[0] != '/')
        return 0;

    /* exact-match mount points only for now. If user mounts "/" it will match "/".
       We also support the case where a mount point is "/" and any path starting with
       "/" should return the mount's root (later we'll parse path components).
    */

    for (int i = 0; i < mount_count; i++) {
        const char* mp = mounts[i].path;
        if (!mp)
            continue;

        /* exact match */
        if (strcmp(path, mp) == 0)
            return mounts[i].root;

        /* if mount point is "/" and path starts with "/", return root */
        if (strcmp(mp, "/") == 0)
            return mounts[i].root;

        /* future: match prefix mount points (e.g. /mnt/foo -> /mnt/foo/bar) */
    }

    return 0;
}
