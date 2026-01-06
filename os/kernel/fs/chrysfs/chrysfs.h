#pragma once
#include "../../storage/block.h"

#ifdef __cplusplus
extern "C" {
#endif

void chrysfs_init(void);
int chrysfs_mount(block_device_t *dev, const char *mountpoint);
int chrysfs_format(block_device_t *dev);

// Simple operations
int chrysfs_ls(const char *path);
int chrysfs_create_file(const char *path, const void *data, uint32_t size);
int chrysfs_read_file(const char *path, void *buf, uint32_t max_size);

#ifdef __cplusplus
}
#endif