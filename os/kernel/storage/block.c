#include "block.h"
#include "../string.h"

extern void serial(const char *fmt, ...);

#define MAX_BLOCK_DEVICES 16
static block_device_t *devices[MAX_BLOCK_DEVICES];
static int dev_count = 0;

void block_init(void) {
    for(int i=0; i<MAX_BLOCK_DEVICES; i++) devices[i] = 0;
    dev_count = 0;
    serial("[BLOCK] subsystem initialized\n");
}

int block_register(block_device_t *dev) {
    if (dev_count >= MAX_BLOCK_DEVICES) return -1;
    
    devices[dev_count] = dev;
    serial("[BLOCK] registered device: %s (%llu sectors)\n", dev->name, (unsigned long long)dev->sector_count);
    dev_count++;
    return 0;
}

block_device_t* block_get(const char* name) {
    for(int i=0; i<dev_count; i++) {
        if (strcmp(devices[i]->name, name) == 0) {
            return devices[i];
        }
    }
    return 0;
}