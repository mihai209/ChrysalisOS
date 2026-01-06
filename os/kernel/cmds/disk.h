#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Partition assign table (volatile, only in-memory) */
struct part_assign {
    int used;
    char letter;
    uint32_t lba;
    uint32_t count;
};

extern struct part_assign g_assigns[26];

/* Declarații externe pentru funcțiile folosite și în fat.cpp */
extern int create_minimal_mbr(void);
extern void cmd_scan(void);
extern int cmd_format_letter(char letter);

/* Shell command entrypoint - stil VECHI */
void cmd_disk(int argc, char** argv);

#ifdef __cplusplus
}
#endif