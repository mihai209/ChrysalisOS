#pragma once

#include "../vfs/vnode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* returns the ramfs root vnode (statically allocated) */
struct vnode* ramfs_root(void);

#ifdef __cplusplus
}
#endif
