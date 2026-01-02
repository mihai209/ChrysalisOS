#pragma once

#include "vnode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* mount a filesystem root at path (string pointer must remain valid) */
void vfs_mount(const char* path, vnode_t* root);

/* resolve a path to a vnode. Minimal resolver: matches mount points exactly.
   Returns vnode pointer or NULL if not found. */
vnode_t* vfs_resolve(const char* path);

#ifdef __cplusplus
}
#endif
