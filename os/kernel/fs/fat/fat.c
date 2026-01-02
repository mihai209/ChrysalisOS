/* freestanding FAT reader (minimal) - no libc headers */
/*#include "../../storage/ata.h"*/
#include "fat.h"
#include "../../terminal.h"

/* Forward declaration only for ATA function used here.
   Do NOT forward-declare terminal_writestring because terminal.h
   already declares it (with const char*). */
int ata_read_sector(uint32_t lba, uint8_t *buf);

/* Local integer helpers (already in fat.h types) */
static inline uint16_t read_u16_le(const uint8_t *b, int off) {
    return (uint16_t)b[off] | ((uint16_t)b[off+1] << 8);
}
static inline uint32_t read_u32_le(const uint8_t *b, int off) {
    return (uint32_t)b[off] | ((uint32_t)b[off+1] << 8) | ((uint32_t)b[off+2] << 16) | ((uint32_t)b[off+3] << 24);
}

/* u32 -> decimal string */
static char* u32_to_dec_local(char *out, uint32_t v) {
    char tmp[12]; int ti = 0;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return out + 1; }
    while (v > 0) { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; }
    int j = 0;
    for (int i = ti - 1; i >= 0; --i) out[j++] = tmp[i];
    out[j] = '\0';
    return out + j;
}

/* println wrapper */
static void tprintln(const char *s) {
    terminal_writestring(s);
    terminal_writestring("\n");
}

/* BPB fields we care about */
static uint16_t b_bytes_per_sector;
static uint8_t  b_sectors_per_cluster;
static uint16_t b_reserved_sectors;
static uint8_t  b_fat_count;
static uint16_t b_root_entry_count;
static uint16_t b_sectors_per_fat;
static uint32_t b_total_sectors;

/* mount state */
static uint32_t fat_lba_base = 0;
static int fat_mounted = 0;

bool fat_mount(uint32_t lba_start)
{
    uint8_t sector[512];
    if (ata_read_sector(lba_start, sector) != 0) {
        tprintln("[fat] read failed");
        return false;
    }

    b_bytes_per_sector    = read_u16_le(sector, 11);
    b_sectors_per_cluster = sector[13];
    b_reserved_sectors    = read_u16_le(sector, 14);
    b_fat_count           = sector[16];
    b_root_entry_count    = read_u16_le(sector, 17);
    b_sectors_per_fat     = read_u16_le(sector, 22);

    uint16_t tot16 = read_u16_le(sector, 19);
    uint32_t tot32 = read_u32_le(sector, 32);
    if (tot16 != 0) b_total_sectors = tot16; else b_total_sectors = tot32;

    if (b_bytes_per_sector != 512) {
        tprintln("[fat] unsupported sector size");
        return false;
    }
    if (b_sectors_per_fat == 0 || b_fat_count == 0) {
        tprintln("[fat] not FAT16 or invalid BPB");
        return false;
    }

    fat_lba_base = lba_start;
    fat_mounted = 1;
    tprintln("[fat] mounted");
    return true;
}

void fat_info(void)
{
    if (!fat_mounted) {
        tprintln("[fat] not mounted");
        return;
    }

    char tmp[64];

    terminal_writestring("[FAT] bytes/sector = ");
    u32_to_dec_local(tmp, b_bytes_per_sector);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] sectors/cluster = ");
    u32_to_dec_local(tmp, b_sectors_per_cluster);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] reserved = ");
    u32_to_dec_local(tmp, b_reserved_sectors);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] fats = ");
    u32_to_dec_local(tmp, b_fat_count);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] root entries = ");
    u32_to_dec_local(tmp, b_root_entry_count);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] sectors/fat = ");
    u32_to_dec_local(tmp, b_sectors_per_fat);
    terminal_writestring(tmp);
    terminal_writestring("\n");

    terminal_writestring("[FAT] total sectors = ");
    u32_to_dec_local(tmp, b_total_sectors);
    terminal_writestring(tmp);
    terminal_writestring("\n");
}
