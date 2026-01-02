#include "ramfs.h"
#include "../vfs/fs_ops.h"
#include "../vfs/vnode.h"
#include <stdint.h>

/* Minimal ramfs: root directory only, no children. Implements open/read/write as no-ops. */

static int ramfs_open(struct vnode* n)
{
    (void)n;
    return 0;
}

static int ramfs_read(struct vnode* n, uint32_t off, uint8_t* buf, uint32_t size)
{
    (void)n; (void)off; (void)buf; (void)size;
    return 0; /* nothing to read */
}

static int ramfs_write(struct vnode* n, uint32_t off, const uint8_t* buf, uint32_t size)
{
    (void)n; (void)off; (void)buf; (void)size;
    return 0; /* not writable in this minimal impl */
}

static int ramfs_readdir(struct vnode* dir, uint32_t index, struct vnode** out)
{
    (void)dir; (void)index;
    if (out) *out = NULL;
    return 0; /* no entries */
}

static fs_ops_t ramfs_ops = {
    .open = ramfs_open,
    .read = ramfs_read,
    .write = ramfs_write,
    .readdir = ramfs_readdir
};

static vnode_t ramfs_root_node = {
    .name = "/",
    .type = VNODE_DIR,
    .ops = &ramfs_ops,
    .internal = 0,
    .parent = 0
};

struct vnode* ramfs_root(void)
{
    return &ramfs_root_node;
}
