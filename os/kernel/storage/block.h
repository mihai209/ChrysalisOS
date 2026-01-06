#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct block_device {
    char name[32];
    uint64_t sector_count;
    uint32_t sector_size;
    int (*read)(struct block_device *dev, uint64_t lba, uint32_t count, void *buf);
    int (*write)(struct block_device *dev, uint64_t lba, uint32_t count, const void *buf);
    void *priv; // Driver private data
} block_device_t;

void block_init(void);
int block_register(block_device_t *dev);
block_device_t* block_get(const char* name);

#ifdef __cplusplus
}
#endif