/* os/kernel/apps/icons/icons.h */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Icon IDs - Shared between win.cpp and icons.mod */
enum {
    ICON_START = 0,
    ICON_TERM,
    ICON_FILES,
    ICON_IMG,
    ICON_NOTE,
    ICON_PAINT,
    ICON_CALC,
    ICON_CLOCK,
    ICON_CAL,
    ICON_TASK,
    ICON_INFO,
    ICON_3D,
    ICON_MINE,
    ICON_DOOM,
    ICON_NET,
    ICON_XO,
    ICON_RUN
};

typedef struct {
    uint16_t w;
    uint16_t h;
    const uint32_t* pixels; /* ARGB/RGBA buffer */
} icon_image_t;

/* Initialize the icon subsystem by loading the module file */
bool icons_init(const char* path);

/* Retrieve an icon by its ID. Returns NULL if not found. */
const icon_image_t* icon_get(uint16_t id);

#ifdef __cplusplus
}
#endif
