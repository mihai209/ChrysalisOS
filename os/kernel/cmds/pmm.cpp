// kernel/cmds/pmm.cpp
#include "pmm.h"
#include "../memory/pmm.h"
#include "../terminal.h"
#include <stdint.h>

/* Minimal strcmp replacement (kernel-safe) */
static int kstrcmp(const char* a, const char* b)
{
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* simple helper to print usage/help */
static void pmm_print_help()
{
    terminal_writestring("pmm — Physical Memory Manager inspector\n");
    terminal_writestring("Usage: pmm [option]\n");
    terminal_writestring("Options:\n");
    terminal_writestring("  --help           Show this help\n");
    terminal_writestring("  --status         Show frames total/used/free and percent\n");
    terminal_writestring("  --free           Show free frames and free bytes\n");
    terminal_writestring("  --usage          Show usage percent and used/total frames\n");
    terminal_writestring("  --max-ram        Show total RAM detected (KB / MB)\n");
    terminal_writestring("  --see-allocated  List allocated regions (compact ranges)\n");
}

/* print numeric sizes in human readable */
static void print_bytes_human(uint64_t bytes)
{
    if (bytes >= 1024ull * 1024ull) {
        uint32_t mb = (uint32_t)(bytes / (1024ull * 1024ull));
        terminal_printf("%u MB", mb);
    } else if (bytes >= 1024ull) {
        uint32_t kb = (uint32_t)(bytes / 1024ull);
        terminal_printf("%u KB", kb);
    } else {
        terminal_printf("%u B", (uint32_t)bytes);
    }
}

/* compact-scan allocated frames and print ranges start..end (physical addresses) */
static void pmm_print_allocated_ranges()
{
    uint32_t total = pmm_total_frames();
    if (total == 0) {
        terminal_writestring("No frames available / PMM not initialized.\n");
        return;
    }

    uint32_t i = 0;
    uint32_t printed_ranges = 0;
    for ( ; i < total; ) {
        int used = pmm_is_frame_used(i);
        if (used == -1) break; /* out of range safety */

        if (!used) { i++; continue; }

        /* start of a used range */
        uint32_t start = i;
        uint32_t j = i + 1;
        for (; j < total; j++) {
            int u2 = pmm_is_frame_used(j);
            if (u2 != 1) break;
        }
        uint32_t end = j - 1;

        /* print range as physical addresses */
        uint32_t start_addr = start * PAGE_SIZE;
        uint32_t end_addr = (end * PAGE_SIZE) + (PAGE_SIZE - 1);

        terminal_printf("0x%x - 0x%x (%u pages)\n", start_addr, end_addr, end - start + 1);

        printed_ranges++;
        i = j;
    }

    if (printed_ranges == 0) {
        terminal_writestring("No allocated frames found.\n");
    }
}

/* command entry */
int cmd_pmm(int argc, char** argv)
{
    if (argc < 2) {
        pmm_print_help();
        return 0;
    }

    const char* opt = argv[1];

    if (kstrcmp(opt, "--help") == 0) {
        pmm_print_help();
        return 0;
    }

    if (kstrcmp(opt, "--status") == 0) {
        uint32_t total = pmm_total_frames();
        uint32_t used = pmm_used_frames();
        uint32_t free = (total > used) ? (total - used) : 0;
        uint32_t percent = (total == 0) ? 0 : (used * 100u) / total;

        terminal_printf("PMM status: %u total frames, %u used, %u free (%u%% used)\n",
                        total, used, free, percent);
        uint64_t total_bytes = (uint64_t)total * PAGE_SIZE;
        uint64_t free_bytes  = (uint64_t)free  * PAGE_SIZE;
        terminal_writestring("Total RAM: ");
        print_bytes_human(total_bytes);
        terminal_writestring(", Free: ");
        print_bytes_human(free_bytes);
        terminal_writestring("\n");
        return 0;
    }

    if (kstrcmp(opt, "--free") == 0) {
        uint32_t total = pmm_total_frames();
        uint32_t used = pmm_used_frames();
        uint32_t free = (total > used) ? (total - used) : 0;
        uint64_t free_bytes  = (uint64_t)free * PAGE_SIZE;
        terminal_printf("%u free frames\n", free);
        terminal_writestring("Free bytes: ");
        print_bytes_human(free_bytes);
        terminal_writestring("\n");
        return 0;
    }

    if (kstrcmp(opt, "--usage") == 0) {
        uint32_t total = pmm_total_frames();
        uint32_t used = pmm_used_frames();
        uint32_t percent = (total == 0) ? 0 : (used * 100u) / total;
        terminal_printf("Usage: %u%% (%u / %u frames)\n", percent, used, total);
        return 0;
    }

    if (kstrcmp(opt, "--max-ram") == 0) {
        uint32_t total = pmm_total_frames();
        uint64_t total_bytes = (uint64_t)total * PAGE_SIZE;
        terminal_writestring("Detected RAM: ");
        print_bytes_human(total_bytes);
        terminal_writestring("\n");
        return 0;
    }

    if (kstrcmp(opt, "--see-allocated") == 0) {
        terminal_writestring("Allocated regions (physical addresses):\n");
        pmm_print_allocated_ranges();
        return 0;
    }

    terminal_writestring("Unknown option. Use --help for usage.\n");
    return 0;
}
