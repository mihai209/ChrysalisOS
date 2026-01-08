/* kernel/cmds/fat.cpp */
#include "fat.h"
#include "disk.h" // Acces la g_assigns
#include "../fs/fat/fat.h"
#include "../terminal.h"
#include "../string.h"
#include "../mem/kmalloc.h"

extern void terminal_printf(const char* fmt, ...);

static bool is_fat_initialized = false;
static uint32_t current_lba = 0;
static char current_letter = 0;

/* --- FAT32 Structures & Helpers (Local Implementation) --- */

struct fat_bpb {
    uint8_t jmp[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fats_count;
    uint16_t root_entries_count;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
} __attribute__((packed));

struct fat_dir_entry {
    char name[11];
    uint8_t attr;
    uint8_t reserved;
    uint8_t ctime_tenth;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t cluster_hi;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed));

/* Convert path to 8.3 DOS name (e.g. "test.txt" -> "TEST    TXT") */
static void to_dos_name(const char* path, char* dst) {
    memset(dst, ' ', 11);
    
    /* Skip directory part if present */
    const char* p = path;
    const char* last_slash = 0;
    while (*p) { if (*p == '/') last_slash = p; p++; }
    if (last_slash) path = last_slash + 1;
    
    /* Name */
    int i = 0;
    for (; i < 8 && path[i] && path[i] != '.'; i++) {
        char c = path[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[i] = c;
    }
    
    /* Extension */
    while (path[i] && path[i] != '.') i++;
    if (path[i] == '.') {
        i++;
        int j = 8;
        for (; j < 11 && path[i]; i++, j++) {
            char c = path[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            dst[j] = c;
        }
    }
}

/* --- FAT32 File Operations --- */

extern "C" int fat32_read_file(const char* path, void* buf, uint32_t max_size) {
    if (!is_fat_initialized) return -1;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    /* 1. Read BPB */
    if (disk_read_sector(current_lba, sector) != 0) { kfree(sector); return -1; }
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;
    uint16_t bps = bpb->bytes_per_sector;

    /* 2. Find File in Root Directory */
    char target[11];
    to_dos_name(path, target);

    uint32_t current_cluster = root_cluster;
    uint32_t file_cluster = 0;
    uint32_t file_size = 0;
    bool found = false;

    /* Simple scan of root cluster (first cluster only for simplicity) */
    uint32_t lba = data_start + (current_cluster - 2) * spc;
    for (int i = 0; i < spc; i++) {
        disk_read_sector(lba + i, sector);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)sector;
        for (int j = 0; j < 512 / 32; j++) {
            if (entries[j].name[0] == 0) break;
            if (entries[j].name[0] == 0xE5) continue;
            if (entries[j].attr & 0x0F) continue; /* Skip LFN/Dir/Vol */

            if (memcmp(entries[j].name, target, 11) == 0) {
                file_cluster = (entries[j].cluster_hi << 16) | entries[j].cluster_low;
                file_size = entries[j].size;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) { kfree(sector); return -1; }

    /* 3. Read File Data */
    uint32_t bytes_read = 0;
    uint8_t* out = (uint8_t*)buf;
    current_cluster = file_cluster;

    while (bytes_read < file_size && bytes_read < max_size) {
        uint32_t cluster_lba = data_start + (current_cluster - 2) * spc;
        
        for (int i = 0; i < spc; i++) {
            disk_read_sector(cluster_lba + i, sector);
            uint32_t chunk = (file_size - bytes_read > 512) ? 512 : (file_size - bytes_read);
            if (bytes_read + chunk > max_size) chunk = max_size - bytes_read;
            
            memcpy(out + bytes_read, sector, chunk);
            bytes_read += chunk;
            if (bytes_read >= file_size || bytes_read >= max_size) break;
        }

        /* Get next cluster from FAT */
        uint32_t fat_sector = fat_start + (current_cluster * 4) / bps;
        uint32_t fat_offset = (current_cluster * 4) % bps;
        
        disk_read_sector(fat_sector, sector);
        current_cluster = (*(uint32_t*)(sector + fat_offset)) & 0x0FFFFFFF;

        if (current_cluster >= 0x0FFFFFF8) break; /* EOC */
    }

    kfree(sector);
    return bytes_read;
}

extern "C" int fat32_create_file(const char* path, const void* data, uint32_t size) {
    if (!is_fat_initialized) return -1;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    /* 1. Read BPB */
    disk_read_sector(current_lba, sector);
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;
    uint16_t bps = bpb->bytes_per_sector;

    char target[11];
    to_dos_name(path, target);

    /* 2. Find or Create Directory Entry */
    uint32_t dir_lba = data_start + (root_cluster - 2) * spc;
    struct fat_dir_entry* target_entry = 0;
    uint32_t entry_sector_lba = 0;
    
    /* Scan root dir for existing file or free slot */
    for (int i = 0; i < spc; i++) {
        disk_read_sector(dir_lba + i, sector);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)sector;
        
        for (int j = 0; j < 512 / 32; j++) {
            /* Check for match */
            if (memcmp(entries[j].name, target, 11) == 0) {
                target_entry = &entries[j];
                entry_sector_lba = dir_lba + i;
                break;
            }
            /* Check for free slot if not found yet */
            if ((entries[j].name[0] == 0 || entries[j].name[0] == 0xE5) && !target_entry) {
                target_entry = &entries[j];
                entry_sector_lba = dir_lba + i;
                /* Don't break, keep looking for match in case it exists later */
            }
        }
        if (target_entry && memcmp(target_entry->name, target, 11) == 0) break;
    }

    if (!target_entry) { kfree(sector); return -1; /* Root dir full */ }

    /* 3. Allocate Cluster (Simple: Find first free in FAT) */
    /* Note: If file exists, we reuse its cluster. If new, we alloc. */
    uint32_t file_cluster = (target_entry->cluster_hi << 16) | target_entry->cluster_low;
    
    if (file_cluster == 0) {
        /* Find free cluster */
        uint32_t fat_sector_0 = fat_start;
        for (uint32_t s = 0; s < bpb->sectors_per_fat_32; s++) {
            disk_read_sector(fat_sector_0 + s, sector);
            uint32_t* table = (uint32_t*)sector;
            for (int k = 0; k < 128; k++) {
                if ((table[k] & 0x0FFFFFFF) == 0) {
                    file_cluster = s * 128 + k;
                    /* Mark as EOC */
                    table[k] = 0x0FFFFFFF;
                    disk_write_sector(fat_sector_0 + s, sector);
                    goto alloc_done;
                }
            }
        }
        kfree(sector); return -2; /* Disk full */
    }
alloc_done:

    /* 4. Write Data */
    uint32_t cluster_lba = data_start + (file_cluster - 2) * spc;
    const uint8_t* data_ptr = (const uint8_t*)data;
    uint32_t written = 0;

    /* Write only first cluster (limit for simplicity) */
    for (int i = 0; i < spc && written < size; i++) {
        memset(sector, 0, 512);
        uint32_t chunk = (size - written > 512) ? 512 : (size - written);
        memcpy(sector, data_ptr + written, chunk);
        disk_write_sector(cluster_lba + i, sector);
        written += chunk;
    }

    /* 5. Update Directory Entry */
    disk_read_sector(entry_sector_lba, sector);
    /* Re-locate entry pointer in refreshed buffer */
    /* Note: simplistic, assumes sector didn't change structure, valid for single task */
    /* We need to find the offset again or just use the pointer if we are careful */
    /* To be safe, we search again in the loaded sector */
    struct fat_dir_entry* entries = (struct fat_dir_entry*)sector;
    struct fat_dir_entry* entry = 0;
    
    /* Try to find by name first (update) */
    for(int j=0; j<16; j++) if(memcmp(entries[j].name, target, 11) == 0) entry = &entries[j];
    
    /* If not found, find the free slot we identified earlier (create) */
    /* This part is tricky without offset. Let's assume we are creating new if size was 0 */
    if (!entry) {
        for(int j=0; j<16; j++) {
            if (entries[j].name[0] == 0 || entries[j].name[0] == 0xE5) {
                entry = &entries[j];
                break;
            }
        }
    }

    if (entry) {
        memcpy(entry->name, target, 11);
        entry->attr = 0x20; /* Archive */
        entry->cluster_hi = (file_cluster >> 16);
        entry->cluster_low = (file_cluster & 0xFFFF);
        entry->size = size;
        disk_write_sector(entry_sector_lba, sector);
    }

    kfree(sector);
    return 0;
}

/* Încearcă să monteze automat prima partiție FAT găsită */
void fat_automount(void) {
    if (is_fat_initialized) return;

    /* 1. Verificăm dacă avem partiții detectate. Dacă nu, scanăm. */
    bool has_partitions = false;
    for (int i = 0; i < 26; i++) {
        if (g_assigns[i].used) {
            has_partitions = true;
            break;
        }
    }

    if (!has_partitions) {
        // terminal_writestring("[AutoMount] Probing partitions...\n");
        disk_probe_partitions();
    }

    /* 2. Căutăm prima partiție de tip FAT32 (0x0B sau 0x0C) */
    for (int i = 0; i < 26; i++) {
        if (g_assigns[i].used) {
            uint8_t t = g_assigns[i].type;
            if (t == 0x0B || t == 0x0C) {
                terminal_printf("[AutoMount] Mounting FAT32 on partition %c (LBA %u)...\n", g_assigns[i].letter, g_assigns[i].lba);
                if (fat32_init(0, g_assigns[i].lba) == 0) {
                    is_fat_initialized = true;
                    current_lba = g_assigns[i].lba;
                    current_letter = g_assigns[i].letter;
                    return;
                }
            }
        }
    }
}

static void cmd_usage(void) {
    terminal_writestring("Usage: fat <command>\n");
    terminal_writestring("Commands:\n");
    terminal_writestring("  mount <part>   Mount FAT32 on partition letter (e.g. 'a')\n");
    terminal_writestring("  ls             List root directory\n");
    terminal_writestring("  info           Show filesystem info\n");
}

extern "C" int cmd_fat(int argc, char **argv) {
    if (argc < 2) {
        cmd_usage();
        return -1;
    }

    const char* sub = argv[1];

    if (strcmp(sub, "ls") == 0) {
        fat_automount();
        if (is_fat_initialized) {
            terminal_printf("Listing root of FAT32 (Partition %c, LBA %u):\n", 
                            current_letter ? current_letter : '?', current_lba);
            fat32_list_root();
        }
        else terminal_writestring("FAT not mounted (no FAT32 partition found).\n");
        return 0;
    }

    if (strcmp(sub, "info") == 0) {
        fat_automount();
        if (is_fat_initialized) {
            terminal_writestring("FAT32 Filesystem Status:\n");
            terminal_writestring("  State: Mounted\n");
            terminal_printf("  Partition: %c\n", current_letter ? current_letter : '?');
            terminal_printf("  LBA Start: %u\n", current_lba);
        } else terminal_writestring("FAT not mounted.\n");
        return 0;
    }

    if (strcmp(sub, "mount") == 0) {
        if (argc < 3) {
            terminal_writestring("Usage: fat mount <partition_letter>\n");
            return -1;
        }
        
        char letter = argv[2][0];
        if (letter >= 'A' && letter <= 'Z') letter += 32; // to lowercase
        
        int idx = letter - 'a';
        if (idx < 0 || idx >= 26) {
            terminal_writestring("Invalid partition letter.\n");
            return -1;
        }

        if (!g_assigns[idx].used) {
            terminal_printf("Partition '%c' is not assigned. Run 'disk probe' first.\n", letter);
            return -1;
        }

        uint32_t lba = g_assigns[idx].lba;
        terminal_printf("Mounting FAT32 on partition %c (LBA %u)...\n", letter, lba);
        
        // 0 = device ID (ignorat dacă fat.c folosește disk_read_sector global)
        if (fat32_init(0, lba) == 0) {
            terminal_writestring("Mount successful.\n");
            is_fat_initialized = true;
            current_lba = lba;
            current_letter = letter;
        } else {
            terminal_writestring("Mount failed.\n");
            is_fat_initialized = false;
        }
        return 0;
    }

    cmd_usage();
    return -1;
}