/* os/kernel/apps/icons/icons.c */
#include "icons.h"
#include "../../mem/kmalloc.h"
#include "../../cmds/fat.h"
#include "../../string.h"

#define ICONS_MAGIC 0x4E4F4349  /* 'ICON' in ASCII */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
} icons_mod_header_t;

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t w;
    uint16_t h;
    uint32_t offset;  /* Offset to pixel data relative to file start */
} icons_mod_entry_t;

static struct {
    uint16_t count;
    const icons_mod_entry_t* entries;
    const uint8_t* base;
    void* file_data;
} g_icons = {0, 0, 0, 0};

bool icons_init(const char* path) {
    /* Ensure filesystem is ready */
    fat_automount();

    int32_t size = fat32_get_file_size(path);
    if (size <= 0) return false;

    void* data = kmalloc(size);
    if (!data) return false;

    if (fat32_read_file(path, data, size) != size) {
        kfree(data);
        return false;
    }

    icons_mod_header_t* h = (icons_mod_header_t*)data;
    if (h->magic != ICONS_MAGIC) {
        kfree(data);
        return false;
    }

    g_icons.count     = h->count;
    g_icons.entries   = (const icons_mod_entry_t*)(h + 1);
    g_icons.base      = (const uint8_t*)data;
    g_icons.file_data = data;

    return true;
}

const icon_image_t* icon_get(uint16_t id) {
    static icon_image_t img;

    if (!g_icons.base) return 0;

    for (int i = 0; i < g_icons.count; i++) {
        if (g_icons.entries[i].id == id) {
            img.w = g_icons.entries[i].w;
            img.h = g_icons.entries[i].h;
            img.pixels = (const uint32_t*)(g_icons.base + g_icons.entries[i].offset);
            return &img;
        }
    }
    return 0;
}
