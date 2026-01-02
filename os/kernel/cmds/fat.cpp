// kernel/cmds/fat.cpp - formatare FAT32 completă + rezolvare erori compilare
#include "fat.h"

#include "../fs/fat/fat.h"
#include "../terminal.h"
#include "../storage/ata.h"         // <--- NECESAR pentru ata_write_sector
#include "../cmds/disk.h"           // g_assigns, create_minimal_mbr, cmd_scan, cmd_format_letter
#include <stdint.h>

/* Fallback MAX_ASSIGN */
#ifndef MAX_ASSIGN
#define MAX_ASSIGN 26
#endif

/* Declarații externe din disk.cpp */
extern int create_minimal_mbr(void);
extern void cmd_scan(void);
extern int cmd_format_letter(char letter);

/* Prototypes din fs/fat/fat.c */
int fat_probe(uint32_t lba);

/* strcmp local freestanding */
static int strcmp_local(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* u32 → string */
static char* u32_to_dec_local(char *out, uint32_t v) {
    char tmp[12]; int ti = 0;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return out; }
    while (v > 0) { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; }
    int j = 0;
    for (int i = ti - 1; i >= 0; --i) out[j++] = tmp[i];
    out[j] = '\0';
    return out;
}

/* Help */
static void print_help(void) {
    terminal_writestring("fat - gestiune FAT filesystem\n");
    terminal_writestring("Comenzi disponibile:\n");
    terminal_writestring("  fat help                 : afiseaza acest ajutor\n");
    terminal_writestring("  fat list                 : status montare + volume detectate\n");
    terminal_writestring("  fat mount <lba>          : monteaza FAT la LBA specificat\n");
    terminal_writestring("  fat info                 : informatii despre FAT-ul montat\n");
    terminal_writestring("  fat --auto-init          : AUTO: MBR + format FAT32 complet + montare\n");
}

/* Listă volume FAT */
static void list_all_fat_volumes(void) {
    int found = 0;
    terminal_writestring("Volume FAT detectate:\n");

    for (int i = 0; i < MAX_ASSIGN; i++) {
        if (!g_assigns[i].used) continue;

        uint32_t lba = g_assigns[i].lba;
        char letter = g_assigns[i].letter;

        if (fat_probe(lba)) {
            found = 1;
            char num[32];
            u32_to_dec_local(num, lba);

            char letter_str[4] = { letter, ':', ' ', '\0' };

            terminal_writestring((fat_is_mounted() && fat_get_mounted_lba() == lba) ? " * " : "   ");
            terminal_writestring(letter_str);
            terminal_writestring("LBA ");
            terminal_writestring(num);
            terminal_writestring("  (");
            terminal_writestring((fat_is_mounted() && fat_get_mounted_lba() == lba) ? "montat" : "nemontat");
            terminal_writestring(")\n");
        }
    }

    if (!found) {
        terminal_writestring("  Niciun volum FAT detectat.\n");
    }
}

/* Boot sector FAT32 valid */
static void write_fat32_boot_sector(uint8_t* sector, uint32_t hidden_sectors, uint32_t total_sectors) {
    for (int i = 0; i < 512; i++) sector[i] = 0;

    sector[0] = 0xEB; sector[1] = 0x58; sector[2] = 0x90;

    const char oem[] = "MKFS.FAT";
    for (int i = 0; i < 8; i++) sector[3 + i] = oem[i];

    *(uint16_t*)(sector + 11) = 512;
    sector[13] = 16;                        // 8 KB cluster
    *(uint16_t*)(sector + 14) = 32;
    sector[16] = 2;
    *(uint32_t*)(sector + 28) = hidden_sectors;
    *(uint32_t*)(sector + 32) = total_sectors;

    uint32_t spf = 4096;                    // suficient pentru majoritatea discurilor mici/medii
    *(uint32_t*)(sector + 36) = spf;
    *(uint32_t*)(sector + 44) = 2;          // root cluster
    *(uint16_t*)(sector + 48) = 1;          // FSInfo
    *(uint16_t*)(sector + 50) = 6;          // backup

    sector[66] = 0x29;
    *(uint32_t*)(sector + 67) = 0x12345678;
    const char label[] = "NO NAME    ";
    for (int i = 0; i < 11; i++) sector[71 + i] = label[i];
    const char fstype[] = "FAT32   ";
    for (int i = 0; i < 8; i++) sector[82 + i] = fstype[i];

    sector[510] = 0x55;
    sector[511] = 0xAA;
}

/* FSInfo sector */
static void write_fsinfo_sector(uint8_t* sector) {
    for (int i = 0; i < 512; i++) sector[i] = 0;
    *(uint32_t*)(sector + 0)   = 0x41615252;
    *(uint32_t*)(sector + 484) = 0x61417272;
    *(uint32_t*)(sector + 488) = 0xFFFFFFFF;
    *(uint32_t*)(sector + 492) = 3;
    *(uint32_t*)(sector + 508) = 0xAA550000;
}

/* Comandă principală */
int cmd_fat(int argc, char **argv)
{
    if (argc < 2) {
        print_help();
        return 0;
    }

    const char* cmd = argv[1];

    if (strcmp_local(cmd, "help") == 0 || strcmp_local(cmd, "--help") == 0 || strcmp_local(cmd, "-h") == 0) {
        print_help();
        return 0;
    }

    if (strcmp_local(cmd, "list") == 0 || strcmp_local(cmd, "--list") == 0) {
        terminal_writestring("Status FAT:\n");
        if (fat_is_mounted()) {
            char num[32];
            u32_to_dec_local(num, fat_get_mounted_lba());
            terminal_writestring("  Montat curent: LBA ");
            terminal_writestring(num);
            terminal_writestring("\n\n");
        } else {
            terminal_writestring("  Niciun FAT montat.\n\n");
        }
        list_all_fat_volumes();
        return 0;
    }

    /* fat --auto-init */
    if (strcmp_local(cmd, "--auto-init") == 0) {
        terminal_writestring("[fat] Pornire initializare automata FAT32...\n");

        terminal_writestring("[fat] 1. Scriere MBR...\n");
        if (create_minimal_mbr() != 0) {
            terminal_writestring("[fat] Eroare MBR!\n");
            return 0;
        }

        terminal_writestring("[fat] 2. Scanare partitii...\n");
        cmd_scan();

        if (!g_assigns[0].used) {
            terminal_writestring("[fat] Eroare: nici o partitie gasita!\n");
            return 0;
        }

        char letter = g_assigns[0].letter;
        uint32_t part_lba = g_assigns[0].lba;
        uint32_t part_sectors = g_assigns[0].count;

        char num_str[32];
        u32_to_dec_local(num_str, part_lba);
        char letter_str[4] = { letter, ':', ' ', '\0' };

        terminal_writestring("[fat] Partitie: ");
        terminal_writestring(letter_str);
        terminal_writestring("LBA ");
        terminal_writestring(num_str);
        terminal_writestring("\n");

        terminal_writestring("[fat] 3. Formatare FAT32...\n");

        uint8_t sector[512];

        write_fat32_boot_sector(sector, part_lba, part_sectors);
        if (ata_write_sector(part_lba, sector) != 0) {
            terminal_writestring("[fat] Eroare boot sector!\n");
            return 0;
        }

        write_fsinfo_sector(sector);
        if (ata_write_sector(part_lba + 1, sector) != 0) {
            terminal_writestring("[fat] Eroare FSInfo!\n");
            return 0;
        }

        write_fat32_boot_sector(sector, part_lba, part_sectors);
        if (ata_write_sector(part_lba + 6, sector) != 0) {
            terminal_writestring("[fat] Eroare backup boot!\n");
        }

        // Inițializare FAT tables (doar începutul)
        for (int i = 0; i < 512; i++) sector[i] = 0;
        sector[0] = 0xF8; sector[1] = 0xFF; sector[2] = 0xFF; sector[3] = 0x0F;
        sector[4] = 0xFF; sector[5] = 0xFF; sector[6] = 0xFF; sector[7] = 0x0F;
        ata_write_sector(part_lba + 32, sector);
        ata_write_sector(part_lba + 32 + 4096, sector);

        terminal_writestring("[fat] 4. Montare...\n");
        if (fat_mount(part_lba)) {
            terminal_writestring("[fat] SUCCES! Unitate FAT32 gata: ");
            terminal_writestring(letter_str);
            terminal_writestring("\n");
        } else {
            terminal_writestring("[fat] Montare esuata (verifica fat_mount).\n");
        }

        return 0;
    }

    /* mount manual */
    if (strcmp_local(cmd, "mount") == 0) {
        if (argc < 3) {
            terminal_writestring("Eroare: fat mount <lba>\n");
            return 0;
        }
        uint32_t lba = 0;
        const char* p = argv[2];
        while (*p) {
            char c = *p++;
            if (c < '0' || c > '9') {
                terminal_writestring("Eroare: LBA invalid\n");
                return 0;
            }
            lba = lba * 10 + (c - '0');
        }
        if (fat_mount(lba)) {
            char num[32];
            u32_to_dec_local(num, lba);
            terminal_writestring("FAT montat la LBA ");
            terminal_writestring(num);
            terminal_writestring("\n");
        } else {
            terminal_writestring("Eroare montare FAT\n");
        }
        return 0;
    }

    /* info */
    if (strcmp_local(cmd, "info") == 0) {
        if (!fat_is_mounted()) {
            terminal_writestring("Eroare: niciun FAT montat.\n");
            return 0;
        }
        fat_info();
        return 0;
    }

    terminal_writestring("Comanda necunoscuta: ");
    terminal_writestring(cmd);
    terminal_writestring("\n");
    print_help();
    return 0;
}