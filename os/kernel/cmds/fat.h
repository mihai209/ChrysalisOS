#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int cmd_fat(int argc, char **argv);

/* Încearcă să monteze automat prima partiție FAT32 găsită */
void fat_automount(void);

/* Listează conținutul unui director (path="/" pentru root) */
void fat32_list_directory(const char* path);

/* Citește dintr-un fișier de la un offset specificat (pentru fișiere mari) */
int fat32_read_file_offset(const char* path, void* buf, uint32_t size, uint32_t offset);

/* Delete a file from the root directory */
int fat32_delete_file(const char* path);

/* Create a directory in the root directory */
int fat32_create_directory(const char* path);

/* Check if a directory exists */
int fat32_directory_exists(const char* path);

/* Format a partition with FAT32 */
int fat32_format(uint32_t lba, uint32_t sector_count, const char* label);

#ifdef __cplusplus
}
#endif
