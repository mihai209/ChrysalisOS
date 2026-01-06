#include "partition.h"
#include "ahci/ahci.h"
#include "../string.h"

/* MBR Structures */
typedef struct {
    uint8_t status;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed)) mbr_entry_t;

typedef struct {
    uint8_t bootstrap[446];
    mbr_entry_t entries[4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

/* GPT Structures */
typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entries_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_entries_crc32;
} __attribute__((packed)) gpt_header_t;

typedef struct {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t flags;
    uint8_t  name[72];
} __attribute__((packed)) gpt_entry_t;

static uint8_t sector_buf[512];
static uint8_t gpt_entry_buf[512];

int partition_scan(int port, partition_info_t *out_parts, int max_parts) {
    if (ahci_read_lba(port, 0, 1, sector_buf) != 0) return -1;

    mbr_t *mbr = (mbr_t*)sector_buf;
    if (mbr->signature != 0xAA55) {
        serial("[PART] Invalid MBR signature: 0x%04x\n", mbr->signature);
        return 0;
    }

    /* Check for GPT Protective MBR */
    if (mbr->entries[0].type == 0xEE) {
        serial("[PART] GPT Protective MBR detected. Parsing GPT...\n");
        
        /* Read GPT Header at LBA 1 */
        if (ahci_read_lba(port, 1, 1, sector_buf) != 0) return -1;
        gpt_header_t *gpt = (gpt_header_t*)sector_buf;

        if (gpt->signature != 0x5452415020494645ULL) { /* "EFI PART" */
            serial("[PART] Invalid GPT signature.\n");
            return 0;
        }

        int count = 0;
        uint64_t entry_lba = gpt->partition_entries_lba;
        uint32_t num_entries = gpt->num_partition_entries;
        uint32_t entry_size = gpt->partition_entry_size;

        /* Simple parser: assumes entry_size fits in sector or is sector aligned logic needed */
        /* For simplicity, read one sector of entries at a time */
        int entries_per_sector = 512 / entry_size;

        for (uint32_t i = 0; i < num_entries && count < max_parts; i++) {
            if (i % entries_per_sector == 0) {
                if (ahci_read_lba(port, entry_lba + (i / entries_per_sector), 1, gpt_entry_buf) != 0) break;
            }
            
            gpt_entry_t *e = (gpt_entry_t*)(gpt_entry_buf + (i % entries_per_sector) * entry_size);
            
            /* Check if type GUID is zero (unused entry) */
            int empty = 1;
            for(int k=0;k<16;k++) if(e->type_guid[k] != 0) empty = 0;
            
            if (!empty) {
                out_parts[count].start_lba = e->first_lba;
                out_parts[count].sector_count = e->last_lba - e->first_lba + 1;
                out_parts[count].type = 0xFF; // GPT generic
                out_parts[count].is_gpt = 1;
                count++;
            }
        }
        return count;
    } else {
        /* Standard MBR */
        int count = 0;
        for (int i = 0; i < 4 && count < max_parts; i++) {
            if (mbr->entries[i].type != 0) {
                out_parts[count].start_lba = mbr->entries[i].lba_start;
                out_parts[count].sector_count = mbr->entries[i].sector_count;
                out_parts[count].type = mbr->entries[i].type;
                out_parts[count].is_gpt = 0;
                count++;
            }
        }
        return count;
    }
}