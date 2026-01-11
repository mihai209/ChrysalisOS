/* kernel/cmds/fat.cpp */
#include "fat.h"
#include "disk.h" // Acces la g_assigns
#include "../fs/fat/fat.h"
#include "../terminal.h"
#include "../string.h"
#include "../mem/kmalloc.h"

extern void terminal_printf(const char* fmt, ...);
extern "C" void serial(const char *fmt, ...);

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

struct fat_fsinfo {
    uint32_t lead_sig;      // 0x41615252
    uint8_t  reserved1[480];
    uint32_t struc_sig;     // 0x61417272
    uint32_t free_count;    // -1
    uint32_t next_free;     // 2
    uint8_t  reserved2[12];
    uint32_t trail_sig;     // 0xAA550000
} __attribute__((packed));

/* Convert a single filename component to 8.3 DOS name */
static void to_dos_name_component(const char* name, int len, char* dst) {
    memset(dst, ' ', 11);
    int i = 0;
    for (; i < 8 && i < len && name[i] != '.'; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[i] = c;
    }
    while (i < len && name[i] != '.') i++;
    if (i < len && name[i] == '.') {
        i++;
        int j = 8;
        for (; j < 11 && i < len; i++, j++) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            dst[j] = c;
        }
    }
}

/* Helper to find an entry in a directory cluster */
static int find_in_cluster(uint32_t dir_cluster, const char* name, int name_len, 
                           uint32_t data_start, uint32_t fat_start, uint32_t spc, uint32_t bps,
                           uint32_t* out_cluster, uint32_t* out_size, uint32_t* out_sector, uint32_t* out_offset, bool* out_is_dir) 
{
    char target[11];
    to_dos_name_component(name, name_len, target);
    
    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    uint32_t current_cluster = dir_cluster;
    while (current_cluster < 0x0FFFFFF8) {
        uint32_t cluster_lba = data_start + (current_cluster - 2) * spc;
        for (int i = 0; i < (int)spc; i++) {
            disk_read_sector(cluster_lba + i, sector);
            struct fat_dir_entry* entries = (struct fat_dir_entry*)sector;
            for (int j = 0; j < 512 / 32; j++) {
                if (entries[j].name[0] == 0) { kfree(sector); return -1; }
                if ((uint8_t)entries[j].name[0] == 0xE5) continue;
                if (entries[j].attr == 0x0F) continue;

                if (memcmp(entries[j].name, target, 11) == 0) {
                    if (out_cluster) {
                        *out_cluster = (entries[j].cluster_hi << 16) | entries[j].cluster_low;
                        if (*out_cluster == 0) *out_cluster = 0; // Root is 0 or 2 usually, handle carefully
                    }
                    if (out_size) *out_size = entries[j].size;
                    if (out_sector) *out_sector = cluster_lba + i;
                    if (out_offset) *out_offset = j;
                    if (out_is_dir) *out_is_dir = (entries[j].attr & 0x10) ? true : false;
                    kfree(sector);
                    return 0;
                }
            }
        }
        
        /* Next cluster */
        uint32_t fat_sector = fat_start + (current_cluster * 4) / bps;
        uint32_t fat_offset = (current_cluster * 4) % bps;
        disk_read_sector(fat_sector, sector);
        current_cluster = (*(uint32_t*)(sector + fat_offset)) & 0x0FFFFFFF;
    }
    kfree(sector);
    return -1;
}

/* Resolve path to parent directory cluster and final filename component */
static int resolve_parent(const char* path, uint32_t root_cluster, uint32_t data_start, uint32_t fat_start, uint32_t spc, uint32_t bps,
                          uint32_t* out_parent_cluster, const char** out_filename, int* out_filename_len) {
    const char* p = path;
    if (*p == '/') p++;
    
    uint32_t curr_cluster = root_cluster;
    
    while (*p) {
        const char* end = p;
        while (*end && *end != '/') end++;
        int len = end - p;
        
        if (*end == 0) {
            /* Last component */
            *out_parent_cluster = curr_cluster;
            *out_filename = p;
            *out_filename_len = len;
            return 0;
        }
        
        /* Find directory */
        uint32_t next_cluster;
        bool is_dir;
        if (find_in_cluster(curr_cluster, p, len, data_start, fat_start, spc, bps, &next_cluster, NULL, NULL, NULL, &is_dir) != 0) {
            return -1; /* Path not found */
        }
        if (!is_dir) return -1; /* Not a directory */
        
        curr_cluster = next_cluster;
        if (curr_cluster == 0) curr_cluster = root_cluster;
        p = end + 1;
    }
    return -1;
}

/* --- FAT32 File Operations --- */

extern "C" int fat32_read_file(const char* path, void* buf, uint32_t max_size) {
    if (!is_fat_initialized) return -1;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    /* 1. Read BPB */
    if (disk_read_sector(current_lba, sector) != 0) { kfree(sector); return -1; }
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    if (bpb->bytes_per_sector == 0) {
        terminal_writestring("Error: Invalid FAT32 BPB (bytes_per_sector=0).\n");
        kfree(sector);
        return -1;
    }

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;
    uint16_t bps = bpb->bytes_per_sector;

    /* 2. Resolve Path */
    uint32_t file_cluster = 0;
    uint32_t file_size = 0;
    uint32_t parent_cluster;
    const char* fname;
    int fname_len;

    if (resolve_parent(path, root_cluster, data_start, fat_start, spc, bps, &parent_cluster, &fname, &fname_len) != 0) {
        kfree(sector); return -1;
    }

    bool is_dir;
    if (find_in_cluster(parent_cluster, fname, fname_len, data_start, fat_start, spc, bps, &file_cluster, &file_size, NULL, NULL, &is_dir) != 0) {
        kfree(sector); return -1;
    }
    
    if (is_dir) {
        /* Cannot read directory as file */
        kfree(sector); return -1;
    }

    /* 3. Read File Data */
    uint32_t bytes_read = 0;
    uint8_t* out = (uint8_t*)buf;
    uint32_t current_cluster = file_cluster;

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

extern "C" int fat32_read_file_offset(const char* path, void* buf, uint32_t size, uint32_t offset) {
    if (!is_fat_initialized) return -1;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    /* 1. Read BPB */
    if (disk_read_sector(current_lba, sector) != 0) { kfree(sector); return -1; }
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    if (bpb->bytes_per_sector == 0) { kfree(sector); return -1; }

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;
    uint16_t bps = bpb->bytes_per_sector;
    uint32_t cluster_bytes = spc * bps;

    /* 2. Resolve Path */
    uint32_t file_cluster = 0;
    uint32_t file_size = 0;
    uint32_t parent_cluster;
    const char* fname;
    int fname_len;

    if (resolve_parent(path, root_cluster, data_start, fat_start, spc, bps, &parent_cluster, &fname, &fname_len) != 0) {
        kfree(sector); return -1;
    }

    bool is_dir;
    if (find_in_cluster(parent_cluster, fname, fname_len, data_start, fat_start, spc, bps, &file_cluster, &file_size, NULL, NULL, &is_dir) != 0) {
        kfree(sector); return -1;
    }

    if (offset >= file_size) { kfree(sector); return 0; }
    if (offset + size > file_size) size = file_size - offset;

    /* 3. Seek to cluster */
    uint32_t current_cluster = file_cluster;
    uint32_t clusters_to_skip = offset / cluster_bytes;
    uint32_t offset_in_cluster = offset % cluster_bytes;

    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        uint32_t fat_sector = fat_start + (current_cluster * 4) / bps;
        uint32_t fat_offset = (current_cluster * 4) % bps;
        disk_read_sector(fat_sector, sector);
        current_cluster = (*(uint32_t*)(sector + fat_offset)) & 0x0FFFFFFF;
        if (current_cluster >= 0x0FFFFFF8) { kfree(sector); return -1; }
    }

    /* 4. Read Data */
    uint32_t bytes_read = 0;
    uint8_t* out = (uint8_t*)buf;

    while (bytes_read < size) {
        uint32_t cluster_lba = data_start + (current_cluster - 2) * spc;
        uint32_t cluster_offset = (bytes_read == 0) ? offset_in_cluster : 0;
        uint32_t start_sector = cluster_offset / bps;
        uint32_t sector_offset = cluster_offset % bps;

        for (int i = start_sector; i < spc && bytes_read < size; i++) {
            disk_read_sector(cluster_lba + i, sector);
            uint32_t available = bps - sector_offset;
            uint32_t to_copy = (size - bytes_read > available) ? available : (size - bytes_read);
            memcpy(out + bytes_read, sector + sector_offset, to_copy);
            bytes_read += to_copy;
            sector_offset = 0;
        }

        uint32_t fat_sector = fat_start + (current_cluster * 4) / bps;
        uint32_t fat_offset = (current_cluster * 4) % bps;
        disk_read_sector(fat_sector, sector);
        current_cluster = (*(uint32_t*)(sector + fat_offset)) & 0x0FFFFFFF;
        if (current_cluster >= 0x0FFFFFF8) break;
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

    if (bpb->bytes_per_sector == 0) {
        terminal_writestring("Error: Invalid FAT32 BPB (bytes_per_sector=0).\n");
        kfree(sector);
        return -1;
    }

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;
    uint32_t sectors_per_fat = bpb->sectors_per_fat_32;

    /* 2. Resolve Parent Directory */
    uint32_t parent_cluster;
    const char* fname;
    int fname_len;
    if (resolve_parent(path, root_cluster, data_start, fat_start, spc, bpb->bytes_per_sector, &parent_cluster, &fname, &fname_len) != 0) {
        kfree(sector); return -1;
    }

    char target[11];
    to_dos_name_component(fname, fname_len, target);

    uint32_t dir_lba = data_start + (parent_cluster - 2) * spc;
    uint32_t entry_sector_lba = 0;
    uint32_t entry_offset = 0;
    bool found_existing = false;
    bool found_free = false;
    uint32_t file_cluster = 0;
    
    /* Scan root dir for existing file or free slot */
    for (int i = 0; i < spc; i++) {
        disk_read_sector(dir_lba + i, sector);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)sector;
        
        for (int j = 0; j < 512 / 32; j++) {
            /* Check for match */
            if (memcmp(entries[j].name, target, 11) == 0) {
                entry_sector_lba = dir_lba + i;
                entry_offset = j;
                file_cluster = (entries[j].cluster_hi << 16) | entries[j].cluster_low;
                found_existing = true;
                break;
            }
            /* Check for free slot if not found yet */
            if (!found_free && (entries[j].name[0] == 0 || (uint8_t)entries[j].name[0] == 0xE5)) {
                entry_sector_lba = dir_lba + i;
                entry_offset = j;
                found_free = true;
                /* Don't break, keep looking for match in case it exists later */
            }
        }
        if (found_existing) break;
    }

    if (!found_existing && !found_free) { kfree(sector); return -1; /* Root dir full */ }

    /* 3. Allocate Cluster (Simple: Find first free in FAT) */
    /* Note: If file exists, we reuse its cluster. If new, we alloc. */
    if (file_cluster == 0) {
        /* Find free cluster */
        uint32_t fat_sector_0 = fat_start;
        for (uint32_t s = 0; s < sectors_per_fat; s++) {
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
    struct fat_dir_entry* entries = (struct fat_dir_entry*)sector;
    struct fat_dir_entry* entry = &entries[entry_offset];

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

extern "C" void fat32_list_directory(const char* path) {
    if (!is_fat_initialized) {
        terminal_writestring("FAT not mounted.\n");
        return;
    }

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return;

    /* 1. Read BPB */
    disk_read_sector(current_lba, sector);
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    if (bpb->bytes_per_sector == 0) {
        terminal_writestring("Error: Invalid FAT32 BPB (bytes_per_sector=0).\n");
        kfree(sector);
        return;
    }

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;
    uint16_t bps = bpb->bytes_per_sector;

    uint32_t target_cluster = 0;
    const char* display_path = "/";

    /* Resolve path if not root */
    if (path && path[0] && !(path[0] == '/' && path[1] == 0)) {
        display_path = path;
        
        uint32_t parent_cluster;
        const char* fname;
        int fname_len;
        if (resolve_parent(path, root_cluster, data_start, fat_start, spc, bps, &parent_cluster, &fname, &fname_len) != 0) {
            terminal_printf("Path not found: %s\n", path);
            kfree(sector); return;
        }
        
        bool is_dir;
        if (find_in_cluster(parent_cluster, fname, fname_len, data_start, fat_start, spc, bps, &target_cluster, NULL, NULL, NULL, &is_dir) != 0) {
            terminal_printf("Directory not found: %s\n", path);
            kfree(sector);
            return;
        }
        if (!is_dir) {
            terminal_printf("Not a directory: %s\n", path);
            kfree(sector); return;
        }
    } else {
        target_cluster = root_cluster;
    }
    
    if (target_cluster == 0) target_cluster = root_cluster;

    /* List contents of target_cluster */
    terminal_printf("Listing %s:\n", display_path);
    
    uint32_t current_cluster = target_cluster;
    
    while (true) {
        uint32_t cluster_lba = data_start + (current_cluster - 2) * spc;
        
        for (int i = 0; i < spc; i++) {
            disk_read_sector(cluster_lba + i, sector);
            struct fat_dir_entry* entries = (struct fat_dir_entry*)sector;
            
            for (int j = 0; j < 512 / 32; j++) {
                if (entries[j].name[0] == 0) goto done_listing;
                if ((uint8_t)entries[j].name[0] == 0xE5) continue;
                if (entries[j].attr == 0x0F) continue;
                
                char name[13];
                int k = 0;
                for (int m = 0; m < 8; m++) {
                    if (entries[j].name[m] != ' ') name[k++] = entries[j].name[m];
                }
                if (entries[j].name[8] != ' ') {
                    name[k++] = '.';
                    for (int m = 8; m < 11; m++) {
                        if (entries[j].name[m] != ' ') name[k++] = entries[j].name[m];
                    }
                }
                name[k] = 0;
                
                if (entries[j].attr & 0x10) {
                    terminal_printf("  [DIR]  %s\n", name);
                } else {
                    terminal_printf("  [FILE] %s  (%u bytes)\n", name, entries[j].size);
                }
            }
        }
        
        /* Next cluster */
        uint32_t fat_sector = fat_start + (current_cluster * 4) / bps;
        uint32_t fat_offset = (current_cluster * 4) % bps;
        disk_read_sector(fat_sector, sector);
        current_cluster = (*(uint32_t*)(sector + fat_offset)) & 0x0FFFFFFF;
        
        if (current_cluster >= 0x0FFFFFF8) break;
    }

done_listing:
    kfree(sector);
}

extern "C" int fat32_read_directory(const char* path, fat_file_info_t* out, int max_entries) {
    if (!is_fat_initialized) return 0;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return 0;

    /* 1. Read BPB */
    disk_read_sector(current_lba, sector);
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;
    uint16_t bps = bpb->bytes_per_sector;

    uint32_t target_cluster = 0;

    /* Resolve path (simplified) */
    if (path && path[0] && !(path[0] == '/' && path[1] == 0)) {
        uint32_t parent_cluster;
        const char* fname;
        int fname_len;
        if (resolve_parent(path, root_cluster, data_start, fat_start, spc, bps, &parent_cluster, &fname, &fname_len) != 0) {
            kfree(sector); return 0;
        }
        bool is_dir;
        if (find_in_cluster(parent_cluster, fname, fname_len, data_start, fat_start, spc, bps, &target_cluster, NULL, NULL, NULL, &is_dir) != 0) {
            kfree(sector); return 0;
        }
        if (!is_dir) { kfree(sector); return 0; }
    } else {
        target_cluster = root_cluster;
    }
    
    if (target_cluster == 0) target_cluster = root_cluster;

    int count = 0;
    uint32_t current_cluster = target_cluster;
    
    while (count < max_entries) {
        uint32_t cluster_lba = data_start + (current_cluster - 2) * spc;
        
        for (int i = 0; i < spc && count < max_entries; i++) {
            disk_read_sector(cluster_lba + i, sector);
            struct fat_dir_entry* entries = (struct fat_dir_entry*)sector;
            
            for (int j = 0; j < 512 / 32 && count < max_entries; j++) {
                if (entries[j].name[0] == 0) goto done_reading;
                if ((uint8_t)entries[j].name[0] == 0xE5) continue;
                if (entries[j].attr == 0x0F) continue;
                
                /* Format Name */
                char name[13];
                int k = 0;
                for (int m = 0; m < 8; m++) { if (entries[j].name[m] != ' ') name[k++] = entries[j].name[m]; }
                if (entries[j].name[8] != ' ') {
                    name[k++] = '.';
                    for (int m = 8; m < 11; m++) { if (entries[j].name[m] != ' ') name[k++] = entries[j].name[m]; }
                }
                name[k] = 0;

                memcpy(out[count].name, name, 13);
                out[count].size = entries[j].size;
                out[count].is_dir = (entries[j].attr & 0x10) ? 1 : 0;
                count++;
            }
        }
        
        /* Next cluster logic omitted for brevity (single cluster dir support for now) */
        break; 
    }
done_reading:
    kfree(sector);
    return count;
}

extern "C" int fat32_delete_file(const char* path) {
    if (!is_fat_initialized) return -1;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    disk_read_sector(current_lba, sector);
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    if (bpb->bytes_per_sector == 0) {
        terminal_writestring("Error: Invalid FAT32 BPB (bytes_per_sector=0).\n");
        kfree(sector);
        return -1;
    }

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;
    uint16_t bps = bpb->bytes_per_sector;

    uint32_t parent_cluster;
    const char* fname;
    int fname_len;
    if (resolve_parent(path, root_cluster, data_start, fat_start, spc, bps, &parent_cluster, &fname, &fname_len) != 0) {
        kfree(sector); return -1;
    }

    uint32_t file_cluster = 0;
    uint32_t entry_sector;
    uint32_t entry_offset;
    bool is_dir;
    if (find_in_cluster(parent_cluster, fname, fname_len, data_start, fat_start, spc, bps, &file_cluster, NULL, &entry_sector, &entry_offset, &is_dir) != 0) {
        kfree(sector); return -1;
    }

    /* Mark deleted in directory entry */
    disk_read_sector(entry_sector, sector);
    ((struct fat_dir_entry*)sector)[entry_offset].name[0] = 0xE5;
    disk_write_sector(entry_sector, sector);

    /* Free cluster chain */
    if (file_cluster != 0) {
        uint32_t current = file_cluster;
        while (current < 0x0FFFFFF8 && current != 0) {
            uint32_t fat_sector = fat_start + (current * 4) / bps;
            uint32_t fat_offset = (current * 4) % bps;
            
            disk_read_sector(fat_sector, sector);
            uint32_t next = (*(uint32_t*)(sector + fat_offset)) & 0x0FFFFFFF;
            
            /* Mark free */
            *(uint32_t*)(sector + fat_offset) = 0;
            disk_write_sector(fat_sector, sector);
            
            current = next;
        }
    }

    kfree(sector);
    return 0;
}

extern "C" int fat32_create_directory(const char* path) {
    /* Re-use create_file logic to allocate entry and cluster, then init directory structure */
    /* For simplicity, we create a dummy file first to get a cluster, then convert it to DIR */
    
    /* 1. Create entry as file first (allocates cluster) */
    char dummy[1] = {0};
    if (fat32_create_file(path, dummy, 0) != 0) return -1;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    disk_read_sector(current_lba, sector);
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    if (bpb->bytes_per_sector == 0) {
        terminal_writestring("Error: Invalid FAT32 BPB (bytes_per_sector=0).\n");
        kfree(sector);
        return -1;
    }

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;

    /* 2. Find the entry we just created */
    uint32_t parent_cluster;
    const char* fname;
    int fname_len;
    resolve_parent(path, root_cluster, data_start, fat_start, spc, bpb->bytes_per_sector, &parent_cluster, &fname, &fname_len);

    uint32_t dir_cluster = 0;
    uint32_t entry_sector, entry_offset;
    bool is_dir;
    
    if (find_in_cluster(parent_cluster, fname, fname_len, data_start, fat_start, spc, bpb->bytes_per_sector, &dir_cluster, NULL, &entry_sector, &entry_offset, &is_dir) != 0) {
        kfree(sector); return -1;
    }

    /* Update attribute to DIRECTORY */
    disk_read_sector(entry_sector, sector);
    struct fat_dir_entry* entry = &((struct fat_dir_entry*)sector)[entry_offset];
    entry->attr = 0x10;
    entry->size = 0;
    disk_write_sector(entry_sector, sector);

    if (dir_cluster == 0) { kfree(sector); return -1; }

    /* 3. Initialize the new directory cluster with . and .. */
    uint32_t cluster_lba = data_start + (dir_cluster - 2) * spc;
    
    /* Prepare the first sector with . and .. */
    memset(sector, 0, 512);
    struct fat_dir_entry* dot = (struct fat_dir_entry*)sector;
    
    /* . entry */
    memset(dot[0].name, ' ', 11); dot[0].name[0] = '.'; 
    dot[0].attr = 0x10; 
    dot[0].cluster_hi = (dir_cluster >> 16); dot[0].cluster_low = (dir_cluster & 0xFFFF);
    
    /* .. entry (points to root 0) */
    memset(dot[1].name, ' ', 11); dot[1].name[0] = '.'; dot[1].name[1] = '.';
    dot[1].attr = 0x10;
    
    disk_write_sector(cluster_lba, sector); // Write first sector of new dir
    
    /* Zero out the rest of the sectors in the cluster to avoid garbage entries */
    memset(sector, 0, 512);
    for (int i = 1; i < spc; i++) {
        disk_write_sector(cluster_lba + i, sector);
    }

    kfree(sector);
    return 0;
}

extern "C" int fat32_directory_exists(const char* path) {
    if (!is_fat_initialized) return 0;
    
    /* Root always exists */
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) return 1;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return 0;

    disk_read_sector(current_lba, sector);
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    if (bpb->bytes_per_sector == 0) {
        // Invalid BPB, assume directory doesn't exist
        kfree(sector);
        return 0;
    }

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;

    uint32_t parent_cluster;
    const char* fname;
    int fname_len;
    if (resolve_parent(path, root_cluster, data_start, fat_start, spc, bpb->bytes_per_sector, &parent_cluster, &fname, &fname_len) != 0) {
        kfree(sector); return 0;
    }
    
    bool is_dir;
    int res = find_in_cluster(parent_cluster, fname, fname_len, data_start, fat_start, spc, bpb->bytes_per_sector, NULL, NULL, NULL, NULL, &is_dir);
    
    kfree(sector);
    return (res == 0 && is_dir) ? 1 : 0;
}

extern "C" int fat32_format(uint32_t lba, uint32_t sector_count, const char* label) {
    if (sector_count < 65536) {
        terminal_writestring("Error: Partition too small for FAT32 (need > 32MB approx)\n");
        return -1;
    }

    uint8_t* sector = (uint8_t*)kmalloc_aligned(512, 16);
    if (!sector) return -1;
    memset(sector, 0, 512);

    /* 1. Setup BPB */
    struct fat_bpb* bpb = (struct fat_bpb*)sector;
    
    /* Jump Boot Code */
    bpb->jmp[0] = 0xEB; bpb->jmp[1] = 0x58; bpb->jmp[2] = 0x90;
    memcpy(bpb->oem, "MSWIN4.1", 8);
    
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = 8; // 4KB clusters
    bpb->reserved_sectors = 32;
    bpb->fats_count = 2;
    bpb->root_entries_count = 0; // FAT32
    bpb->total_sectors_16 = 0;
    bpb->media_descriptor = 0xF8;
    bpb->sectors_per_fat_16 = 0;
    bpb->sectors_per_track = 32; // Dummy
    bpb->heads_count = 64;       // Dummy
    bpb->hidden_sectors = lba;
    bpb->total_sectors_32 = sector_count;
    
    /* Calculate FAT size */
    /* Formula: FatSectors = (Total - Res) / (128 * SPC + 2) */
    uint32_t data_sectors = sector_count - bpb->reserved_sectors;
    uint32_t divisor = (128 * bpb->sectors_per_cluster) + 2; // 1026
    uint32_t fat_sectors = (data_sectors + divisor - 1) / divisor;
    
    bpb->sectors_per_fat_32 = fat_sectors;

    /* Save parameters before clearing buffer for next steps */
    uint32_t reserved_sectors = bpb->reserved_sectors;
    uint32_t fats_count = bpb->fats_count;
    uint32_t sectors_per_cluster = bpb->sectors_per_cluster;

    bpb->ext_flags = 0;
    bpb->fs_version = 0;
    bpb->root_cluster = 2;
    bpb->fs_info = 1;
    bpb->backup_boot_sector = 6;
    bpb->drive_number = 0x80;
    bpb->boot_signature = 0x29;
    bpb->volume_id = 0x12345678;
    
    char vol_label[11];
    memset(vol_label, ' ', 11);
    if (label) {
        for(int i=0; i<11 && label[i]; i++) vol_label[i] = label[i];
    } else {
        memcpy(vol_label, "NO NAME    ", 11);
    }
    memcpy(bpb->volume_label, vol_label, 11);
    memcpy(bpb->fs_type, "FAT32   ", 8);
    
    sector[510] = 0x55;
    sector[511] = 0xAA;
    
    /* Write Boot Sector (LBA 0) */
    if (disk_write_sector(lba, sector) != 0) {
        terminal_writestring("Error: Failed to write Boot Sector\n");
        kfree(sector);
        return -1;
    }
    
    /* Write Backup Boot Sector (LBA 6) */
    if (disk_write_sector(lba + 6, sector) != 0) {
        serial("[FAT] Warning: Failed to write Backup Boot Sector\n");
    }
    
    /* 2. Setup FSInfo (LBA 1) */
    memset(sector, 0, 512);
    struct fat_fsinfo* fsinfo = (struct fat_fsinfo*)sector;
    fsinfo->lead_sig = 0x41615252;
    fsinfo->struc_sig = 0x61417272;
    fsinfo->free_count = 0xFFFFFFFF;
    fsinfo->next_free = 0xFFFFFFFF;
    fsinfo->trail_sig = 0xAA550000;
    
    if (disk_write_sector(lba + 1, sector) != 0) serial("[FAT] Warning: Failed to write FSInfo\n");
    disk_write_sector(lba + 7, sector);
    
    /* 3. Initialize FATs */
    /* We only need to init the first sector of each FAT */
    memset(sector, 0, 512);
    uint32_t* fat_table = (uint32_t*)sector;
    fat_table[0] = 0x0FFFFFF8; // Media
    fat_table[1] = 0x0FFFFFFF; // EOC
    fat_table[2] = 0x0FFFFFFF; // EOC for Root Dir
    
    uint32_t fat1_lba = lba + reserved_sectors;
    uint32_t fat2_lba = fat1_lba + fat_sectors;
    
    if (disk_write_sector(fat1_lba, sector) != 0) serial("[FAT] Warning: Failed to write FAT1\n");
    disk_write_sector(fat2_lba, sector);
    
    /* 4. Initialize Root Directory (Cluster 2) */
    /* Data Start = Res + Fats * FatSec */
    uint32_t data_start = lba + reserved_sectors + (fats_count * fat_sectors);
    /* Cluster 2 is at offset 0 from data_start */
    memset(sector, 0, 512);
    for (uint32_t i = 0; i < sectors_per_cluster; i++) {
        if (disk_write_sector(data_start + i, sector) != 0) serial("[FAT] Warning: Failed to write RootDir sector %d\n", i);
    }
    
    kfree(sector);
    return 0;
}

extern "C" int32_t fat32_get_file_size(const char* path) {
    if (!is_fat_initialized) return -1;

    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    /* 1. Read BPB */
    if (disk_read_sector(current_lba, sector) != 0) { kfree(sector); return -1; }
    struct fat_bpb* bpb = (struct fat_bpb*)sector;

    if (bpb->bytes_per_sector == 0) { kfree(sector); return -1; }

    uint32_t fat_start = current_lba + bpb->reserved_sectors;
    uint32_t data_start = fat_start + (bpb->fats_count * bpb->sectors_per_fat_32);
    uint32_t root_cluster = bpb->root_cluster;
    uint8_t spc = bpb->sectors_per_cluster;

    /* 2. Resolve Path */
    uint32_t parent_cluster;
    const char* fname;
    int fname_len;
    if (resolve_parent(path, root_cluster, data_start, fat_start, spc, bpb->bytes_per_sector, &parent_cluster, &fname, &fname_len) != 0) {
        kfree(sector); return -1;
    }

    uint32_t file_size = 0;
    bool is_dir;
    int res = find_in_cluster(parent_cluster, fname, fname_len, data_start, fat_start, spc, bpb->bytes_per_sector, NULL, &file_size, NULL, NULL, &is_dir);

    kfree(sector);
    return (res == 0 && !is_dir) ? (int32_t)file_size : -1;
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
                /* Check for valid boot signature before attempting mount */
                uint8_t* check_buf = (uint8_t*)kmalloc(512);
                if (check_buf) {
                    disk_read_sector(g_assigns[i].lba, check_buf);
                    if (check_buf[510] != 0x55 || check_buf[511] != 0xAA) {
                        kfree(check_buf);
                        continue; /* Skip unformatted partition */
                    }
                    kfree(check_buf);
                }

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
            fat32_list_directory("/");
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