// kernel/cmds/fat.cpp
#include "fat.h"

#include "../fs/fat/fat.h"
#include "../terminal.h"
#include "../cmds/disk.h"   /* optional: for MAX_ASSIGN and extern g_assigns */
#include <stdint.h>

/* If disk.h didn't define MAX_ASSIGN, provide a sensible default */
#ifndef MAX_ASSIGN
#define MAX_ASSIGN 26
#endif

/* prototype (implemented in fs/fat/fat.c) */
int fat_probe(uint32_t lba);

/* mic strcmp local, freestanding */
static int strcmp_local(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* u32 -> decimal string (scrie în buffer, returnează pointer la început) */
static char* u32_to_dec_local(char *out, uint32_t v) {
    char tmp[12]; int ti = 0;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return out; }
    while (v > 0) { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; }
    int j = 0;
    for (int i = ti - 1; i >= 0; --i) out[j++] = tmp[i];
    out[j] = '\0';
    return out;
}

static void print_help(void) {
    terminal_writestring("fat - gestiune FAT (comenzi):\n");
    terminal_writestring("  fat --help           Afiseaza acest ajutor\n");
    terminal_writestring("  fat --list           Arata LBA-ul montat (daca exista)\n");
    terminal_writestring("  fat mount <lba>      Monteaza filesystemul FAT la LBA specificat\n");
    terminal_writestring("  fat info             Afiseaza informatii despre FAT montat\n");
}

/* main command */
int cmd_fat(int argc, char **argv)
{
    if (argc < 2) {
        print_help();
        return 0;
    }

    /* --help */
    if (!strcmp_local(argv[1], "--help") || !strcmp_local(argv[1], "-h")) {
        print_help();
        return 0;
    }

    /* --list */
    if (!strcmp_local(argv[1], "--list") || !strcmp_local(argv[1], "list")) {

        /* fat --list --unmounted */
        if (argc >= 3 && !strcmp_local(argv[2], "--unmounted")) {
            terminal_writestring("FAT volumes found:\n");

            for (int i = 0; i < MAX_ASSIGN; i++) {
                if (!g_assigns[i].used)
                    continue;

                uint32_t lba = g_assigns[i].lba;

                if (fat_probe(lba)) {
                    char tmp[32];
                    terminal_writestring("  FAT @ LBA ");
                    u32_to_dec_local(tmp, lba);
                    terminal_writestring(tmp);
                    terminal_writestring("\n");
                }
            }

            return 0;
        }

        /* default: show mounted */
        if (fat_is_mounted()) {
            char tmp[32];
            u32_to_dec_local(tmp, fat_get_mounted_lba());
            terminal_writestring("Mounted LBA: ");
            terminal_writestring(tmp);
            terminal_writestring("\n");
        } else {
            terminal_writestring("No FAT mounted\n");
        }

        return 0;
    }

    /* mount <lba> */
    if (!strcmp_local(argv[1], "mount")) {
        if (argc < 3) {
            terminal_writestring("fat mount: lipseste LBA\n");
            return 0;
        }

        /* conversie string -> uint32_t (doar cifre) */
        uint32_t lba = 0;
        const char *p = argv[2];
        if (*p == '\0') {
            terminal_writestring("fat mount: LBA invalid\n");
            return 0;
        }
        while (*p) {
            char c = *p++;
            if (c < '0' || c > '9') {
                terminal_writestring("fat mount: LBA invalid\n");
                return 0;
            }
            lba = lba * 10 + (uint32_t)(c - '0');
        }

        if (!fat_mount(lba)) {
            terminal_writestring("FAT mount failed\n");
        } else {
            terminal_writestring("FAT mounted OK\n");
        }
        return 0;
    }

    /* info */
    if (!strcmp_local(argv[1], "info")) {
        fat_info();
        return 0;
    }

    terminal_writestring("Comanda necunoscuta: ");
    terminal_writestring(argv[1]);
    terminal_writestring("\n");

    return 0;
}