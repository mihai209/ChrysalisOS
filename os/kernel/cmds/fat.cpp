/* kernel/cmds/fat.cpp */
#include "fat.h"
#include "disk.h" // Acces la g_assigns
#include "../fs/fat/fat.h"
#include "../terminal.h"
#include "../string.h"

extern void terminal_printf(const char* fmt, ...);

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
        fat32_list_root();
        return 0;
    }

    if (strcmp(sub, "info") == 0) {
        fat32_list_root(); // Fallback la listare momentan
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
        } else {
            terminal_writestring("Mount failed.\n");
        }
        return 0;
    }

    cmd_usage();
    return -1;
}