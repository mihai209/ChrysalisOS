#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void heap_init(void* start, size_t size);

/* Alloc/free API */
void* kmalloc(size_t size);
void  kfree(void* ptr);

/* aligned allocation (falls back to NULL if not possible) */
void* kmalloc_aligned(size_t size, size_t alignment);

/* Optional debug define:
   #define KMALLOC_DEBUG 1
   If defined, kmalloc.c will attempt to call terminal_printf.
*/
#ifdef __cplusplus
}
#endif
