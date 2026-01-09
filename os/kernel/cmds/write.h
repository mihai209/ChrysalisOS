#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* write <path> <text...>
 * Creates file or appends text with a newline.
 * Prioritizes Disk (FAT32) if available, falls back to RAMFS.
 */
int cmd_write(int argc, char** argv);

#ifdef __cplusplus
}
#endif