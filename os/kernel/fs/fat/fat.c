#include "fat.h"
#include "../../storage/ahci/ahci.h"
#include "../../string.h"

/* BPB Structure for FAT32 */
typedef struct {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats_count;
    uint16_t root_entries_count;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    /* FAT32 Extended */
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    uint8_t name[11];
    uint8_t attr;
    uint8_t reserved;
    uint8_t ctime_tenth;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t cluster_hi;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t cluster_lo;
    uint32_t size;
} __attribute__((packed)) fat_dir_entry_t;

static uint8_t fs_buf[512];
static int fs_port = -1;
static uint64_t fs_part_lba = 0;
static uint64_t data_start_lba = 0;
static uint8_t sec_per_clus = 0;
static uint32_t root_clus = 0;

int fat32_init(int port, uint64_t part_lba) {
    if (ahci_read_lba(port, part_lba, 1, fs_buf) != 0) return -1;
    
    fat32_bpb_t *bpb = (fat32_bpb_t*)fs_buf;
    
    /* Basic validation */
    if (bpb->bytes_per_sector != 512) {
        serial("[FAT] Unsupported sector size: %d\n", bpb->bytes_per_sector);
        return -2;
    }
    if (bpb->total_sectors_16 != 0) {
        serial("[FAT] Not FAT32 (total_sectors_16 != 0)\n");
        return -3;
    }

    fs_port = port;
    fs_part_lba = part_lba;
    sec_per_clus = bpb->sectors_per_cluster;
    root_clus = bpb->root_cluster;

    uint32_t fat_size = bpb->sectors_per_fat_32;
    uint64_t fat_start = part_lba + bpb->reserved_sectors;
    data_start_lba = fat_start + (bpb->fats_count * fat_size);

    serial("[FAT] Init OK. Cluster size: %d sectors. Root Cluster: %d\n", sec_per_clus, root_clus);
    serial("[FAT] Data Start LBA: %llu\n", (unsigned long long)data_start_lba);
    
    return 0;
}

void fat32_list_root(void) {
    if (fs_port == -1) return;

    /* Calculate LBA of root cluster */
    /* FirstSectorofCluster = ((N – 2) * BPB_SecPerClus) + FirstDataSector */
    uint64_t root_lba = ((root_clus - 2) * sec_per_clus) + data_start_lba;

    /* Read first sector of root dir */
    if (ahci_read_lba(fs_port, root_lba, 1, fs_buf) == 0) {
        fat_dir_entry_t *entry = (fat_dir_entry_t*)fs_buf;
        serial("[FAT] Root Directory Listing:\n");
        for (int i = 0; i < 16; i++) { /* Check first 16 entries */
            if (entry[i].name[0] == 0) break; /* End of dir */
            if (entry[i].name[0] == 0xE5) continue; /* Deleted */
            if (entry[i].attr & 0x0F) continue; /* LFN skip for now */
            
            serial("  %.11s  (Size: %u bytes)\n", entry[i].name, entry[i].size);
        }
    }
}