#pragma once

/* kernel/mm/vmm.h
 * Virtual Memory Manager helpers for Chrysalis OS
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Avoid redefining PAGE_SIZE if another header already defined it
 * (pmm.h uses #define PAGE_SIZE 4096).
 */
#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000u
#endif

/* Allocate one 4KiB page and return a kernel-virtual pointer to it.
 * The returned page is zeroed.
 */
void* vmm_alloc_page(void);

/* Free a page previously allocated with vmm_alloc_page.
 */
void vmm_free_page(void* page);

/* Map a single 4KiB page into the specified page-directory.
 * - pagedir: kernel-virtual pointer to the page directory (uint32_t*)
 * - vaddr  : virtual address to map (page-aligned)
 * - phys   : physical frame address (page-aligned)
 * - flags  : flags (PAGE_PRESENT | PAGE_RW | PAGE_USER ...)
 */
void vmm_map_page(uint32_t* pagedir, uint32_t vaddr, uint32_t phys, uint32_t flags);

void vmm_unmap_page(uint32_t* pagedir, uint32_t vaddr);

uint32_t vmm_virt_to_phys(void* vaddr);

#ifdef __cplusplus
}
#endif
