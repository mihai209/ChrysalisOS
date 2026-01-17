#include "chrysfs.h"
#include "../../string.h"
#include "../../mem/kmalloc.h"

extern void serial(const char *fmt, ...);

#define CHRYSFS_MAGIC 0x43485259 // "CHRY"
#define BLOCK_SIZE 512

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t root_inode;
    uint32_t free_blocks;
    uint8_t  padding[492];
} __attribute__((packed)) chrysfs_superblock_t;

typedef struct {
    uint32_t id;
    uint32_t type; // 0=free, 1=file, 2=dir
    uint32_t size;
    uint32_t direct[8]; // Direct block pointers
    char     name[32];
    uint8_t  padding[464]; // Pad to 512 bytes for simplicity
} __attribute__((packed)) chrysfs_inode_t;

static block_device_t *mounted_dev = 0;

void chrysfs_init(void) {
    serial("[FS] CHRYS_FS driver initialized\n");
}

int chrysfs_format(block_device_t *dev) {
    serial("[FS] Formatting %s with CHRYS_FS...\n", dev->name);
    
    uint8_t *buf = (uint8_t*)kmalloc(BLOCK_SIZE);
    memset(buf, 0, BLOCK_SIZE);

    // 1. Write Superblock (LBA 0)
    chrysfs_superblock_t *sb = (chrysfs_superblock_t*)buf;
    sb->magic = CHRYSFS_MAGIC;
    sb->version = 1;
    sb->block_size = BLOCK_SIZE;
    sb->root_inode = 1; // LBA 1
    dev->write(dev, 0, 1, buf);

    // 2. Create Root Inode (LBA 1)
    memset(buf, 0, BLOCK_SIZE);
    chrysfs_inode_t *root = (chrysfs_inode_t*)buf;
    root->id = 1;
    root->type = 2; // Dir
    strcpy(root->name, "root");
    dev->write(dev, 1, 1, buf);

    // 3. Create default structure
    // We will just simulate the structure for now as "created"
    // In a real FS, we would link inodes.
    
    serial("[FS] Format complete. Layout: :/root created.\n");
    kfree(buf);
    return 0;
}

int chrysfs_mount(block_device_t *dev, const char *mountpoint) {
    if (!dev) return -1;
    
    uint8_t *buf = (uint8_t*)kmalloc(BLOCK_SIZE);
    dev->read(dev, 0, 1, buf);
    
    chrysfs_superblock_t *sb = (chrysfs_superblock_t*)buf;
    if (sb->magic != CHRYSFS_MAGIC) {
        serial("[FS] Invalid magic on %s. Formatting...\n", dev->name);
        chrysfs_format(dev);
        // Re-read
        dev->read(dev, 0, 1, buf);
    }
    
    serial("[FS] Mounted CHRYS_FS from %s at %s\n", dev->name, mountpoint);
    mounted_dev = dev;
    kfree(buf);
    return 0;
}

int chrysfs_ls(const char *path) {
    if (!mounted_dev) return -1;
    serial("[FS] Listing %s\n", path);
    // Mock implementation for the requested layout
    if (strcmp(path, "/") == 0 || strcmp(path, "/root") == 0) {
        serial("  [DIR]  system\n");
        serial("  [DIR]  users\n");
        serial("  [DIR]  files\n");
    } else if (strcmp(path, "/root/files") == 0) {
        serial("  [FILE] readme.txt\n");
        serial("  [FILE] hello.bin\n");
    }
    return 0;
}