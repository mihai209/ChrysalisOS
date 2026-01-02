#pragma once

/* minimal, freestanding-friendly integer types
   keep these for C builds; they usually don't collide in C++,
   but we must NOT redefine the C++ built-in `bool`. */
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long long uint64_t;

/* Define `bool`, `true`, `false` only for C (not C++) */
#ifndef __cplusplus
typedef int            bool;
#define true 1
#define false 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Mount FAT partition starting at given LBA (returns true on success) */
bool fat_mount(uint32_t lba_start);
/* Print mounted FAT info (uses terminal_writestring) */
void fat_info(void);

#ifdef __cplusplus
}
#endif
