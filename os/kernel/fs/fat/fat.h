#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mount FAT partition starting at given LBA (returns true on success) */
bool fat_mount(uint32_t lba_start);
/* Print mounted FAT info (uses terminal_writestring) */
void fat_info(void);

/* New: query mount state from other code (e.g. shell cmd) */
int  fat_is_mounted(void);         /* returns 1 if mounted, 0 otherwise */
uint32_t fat_get_mounted_lba(void);/* returns LBA base (undefined if not mounted) */
int fat_probe(uint32_t lba);

#ifdef __cplusplus
}
#endif
