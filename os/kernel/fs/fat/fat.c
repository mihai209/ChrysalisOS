/* freestanding FAT reader - cu suport FAT32 complet, fără <string.h> */
#include "fat.h"
#include "../../terminal.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* ATA read */
int ata_read_sector(uint32_t lba, uint8_t *buf);

/* mount state */
static uint32_t fat_lba_base = 0;
static int fat_mounted = 0;
static int fat_type = 0;  // 12, 16 sau 32

int fat_is_mounted(void)
{
    return fat_mounted;
}

uint32_t fat_get_mounted_lba(void)
{
    return fat_lba_base;
}

/* Local helpers */
static inline uint16_t read_u16_le(const uint8_t *b, int off) {
    return (uint16_t)b[off] | ((uint16_t)b[off+1] << 8);
}
static inline uint32_t read_u32_le(const uint8_t *b, int off) {
    return (uint32_t)b[off] | ((uint32_t)b[off+1] << 8) |
           ((uint32_t)b[off+2] << 16) | ((uint32_t)b[off+3] << 24);
}

/* Implementări manuale pentru memcmp și memcpy (freestanding) */
static int memcmp_local(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

static void* memcpy_local(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

static char* u32_to_dec_local(char *out, uint32_t v) {
    char tmp[12]; int ti = 0;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return out + 1; }
    while (v > 0) { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; }
    int j = 0;
    for (int i = ti - 1; i >= 0; --i) out[j++] = tmp[i];
    out[j] = '\0';
    return out + j;
}

static void tprintln(const char *s) {
    terminal_writestring(s);
    terminal_writestring("\n");
}

/* BPB fields */
static uint16_t b_bytes_per_sector;
static uint8_t  b_sectors_per_cluster;
static uint16_t b_reserved_sectors;
static uint8_t  b_fat_count;
static uint32_t b_sectors_per_fat;
static uint32_t b_total_sectors;
static uint32_t b_root_cluster;     // pentru FAT32

/* Probe: verifică dacă există un volum FAT valid */
int fat_probe(uint32_t lba)
{
    uint8_t sector[512];
    if (ata_read_sector(lba, sector) != 0)
        return 0;

    uint16_t bps = read_u16_le(sector, 11);
    if (bps != 512) return 0;

    uint8_t spc = sector[13];
    if (spc == 0) return 0;

    uint8_t fats = sector[16];
    if (fats == 0) return 0;

    // Verificăm FAT32 la offset 82
    if (memcmp_local(sector + 82, "FAT32   ", 8) == 0)
        return 1;

    // Verificăm FAT12/FAT16 la offset 54
    if (memcmp_local(sector + 54, "FAT12   ", 8) == 0 ||
        memcmp_local(sector + 54, "FAT16   ", 8) == 0)
        return 1;

    return 0;
}

/* Mount - suportă FAT32 */
bool fat_mount(uint32_t lba_start)
{
    uint8_t sector[512];
    if (ata_read_sector(lba_start, sector) != 0) {
        tprintln("[fat] read boot sector failed");
        return false;
    }

    b_bytes_per_sector = read_u16_le(sector, 11);
    if (b_bytes_per_sector != 512) {
        tprintln("[fat] unsupported sector size");
        return false;
    }

    b_sectors_per_cluster = sector[13];
    if (b_sectors_per_cluster == 0) {
        tprintln("[fat] invalid sectors per cluster");
        return false;
    }

    b_reserved_sectors = read_u16_le(sector, 14);
    b_fat_count = sector[16];
    if (b_fat_count == 0) {
        tprintln("[fat] invalid FAT count");
        return false;
    }

    // Detectare tip FAT
    if (memcmp_local(sector + 82, "FAT32   ", 8) == 0) {
        fat_type = 32;
        b_sectors_per_fat = read_u32_le(sector, 36);
        b_root_cluster = read_u32_le(sector, 44);
        b_total_sectors = read_u32_le(sector, 32);
        if (b_total_sectors == 0) {
            tprintln("[fat] invalid total sectors (FAT32)");
            return false;
        }
    } else {
        // FAT12/FAT16
        if (memcmp_local(sector + 54, "FAT12   ", 8) == 0) fat_type = 12;
        else if (memcmp_local(sector + 54, "FAT16   ", 8) == 0) fat_type = 16;
        else {
            tprintln("[fat] unknown FAT type");
            return false;
        }

        b_sectors_per_fat = read_u16_le(sector, 22);
        uint16_t tot16 = read_u16_le(sector, 19);
        b_total_sectors = tot16 ? tot16 : read_u32_le(sector, 32);
        b_root_cluster = 0;
    }

    if (b_sectors_per_fat == 0) {
        tprintln("[fat] invalid sectors per FAT");
        return false;
    }

    fat_lba_base = lba_start;
    fat_mounted = 1;

    terminal_writestring("[fat] mounted FAT");
    char tmp[16];
    u32_to_dec_local(tmp, fat_type);
    terminal_writestring(tmp);
    terminal_writestring(" at LBA ");
    u32_to_dec_local(tmp, lba_start);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    return true;
}

void fat_info(void)
{
    if (!fat_mounted) {
        tprintln("[fat] not mounted");
        return;
    }

    char tmp[64];

    terminal_writestring("[FAT] Type: FAT");
    u32_to_dec_local(tmp, fat_type);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] Bytes/sector: ");
    u32_to_dec_local(tmp, b_bytes_per_sector);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] Sectors/cluster: ");
    u32_to_dec_local(tmp, b_sectors_per_cluster);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] Reserved sectors: ");
    u32_to_dec_local(tmp, b_reserved_sectors);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] FAT count: ");
    u32_to_dec_local(tmp, b_fat_count);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] Sectors/FAT: ");
    u32_to_dec_local(tmp, b_sectors_per_fat);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] Total sectors: ");
    u32_to_dec_local(tmp, b_total_sectors);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    if (fat_type == 32) {
        terminal_writestring("[FAT] Root cluster: ");
        u32_to_dec_local(tmp, b_root_cluster);
        terminal_writestring(tmp);
        terminal_writestring("\n");
    }
}