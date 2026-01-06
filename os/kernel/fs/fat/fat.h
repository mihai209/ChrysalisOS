#ifndef FAT_FS_H
#define FAT_FS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Inițializează driverul FAT32 pe un port și o partiție (offset LBA) */
int fat32_init(int port, uint64_t part_lba);

/* Listează fișierele din root directory */
void fat32_list_root(void);

#ifdef __cplusplus
}
#endif

#endif