/* kernel/cmds/fat.cpp */
#include "fat.h"
#include "disk.h" // Acces la g_assigns
#include "../fs/fat/fat.h"
#include "../terminal.h"
#include "../string.h"

extern void terminal_printf(const char* fmt, ...);

static bool is_fat_initialized = false;
static uint32_t current_lba = 0;
static char current_letter = 0;

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