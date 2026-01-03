#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Paging profiles (MB) */
#define PAGING_20_MB   20
#define PAGING_40_MB   40
#define PAGING_60_MB   60
#define PAGING_80_MB   80
#define PAGING_100_MB  100
#define PAGING_120_MB  120

/* Page flags */
#define PAGE_PRESENT   0x1
#define PAGE_RW        0x2
#define PAGE_USER      0x4

/* Init paging with one of the predefined sizes above */
void paging_init(uint32_t identity_map_mb);

/* Map/unmap API (4KB pages). Return 0 on success. */
int paging_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void paging_unmap_page(uint32_t virtual_addr);

/* Debug: get physical address of current page directory */
uint32_t paging_get_page_directory_phys(void);

#ifdef __cplusplus
}
#endif
