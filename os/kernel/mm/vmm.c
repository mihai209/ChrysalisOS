/* kernel/mm/vmm.c
 * Minimal Virtual Memory Manager for Chrysalis OS
 * Implements functions declared in kernel/mm/vmm.h
 * - vmm_alloc_page / vmm_free_page
 * - vmm_map_page / vmm_unmap_page
 * - vmm_virt_to_phys
 *
 * Assumptions (consistent with our paging layout):
 * - KERNEL_BASE maps physical memory into kernel virtual space
 *   (phys 0x0 -> virt KERNEL_BASE).
 * - pmm_alloc_frame()/pmm_free_frame() exist and return/accept
 *   physical 32-bit frame addresses.
 * - get_pte_for()/get_pde_for() helpers exist as declared in paging.h.
 */

#include "vmm.h"
#include "paging.h"
#include "../memory/pmm.h"
#include <stddef.h>
#include <stdint.h>

/* External kernel page-directory (defined in paging.c). We expect the
 * symbol to be a kernel-virtual pointer to the kernel page directory.
 */
extern uint32_t* kernel_page_directory;

/* Convert physical <-> kernel virtual using KERNEL_BASE high mapping.
 * NOTE: this relies on the kernel having mapped physical memory at
 * KERNEL_BASE (see paging layout).
 */
static inline void* phys_to_virt(uint32_t phys)
{
    return (void*)(uintptr_t)(phys + KERNEL_BASE);
}
static inline uint32_t virt_to_phys_addr(void* virt)
{
    return (uint32_t)((uintptr_t)virt - (uintptr_t)KERNEL_BASE);
}

/* Zero a page (simple loop because freestanding). */
static void zero_page(void* p)
{
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i)
        b[i] = 0;
}

void* vmm_alloc_page(void)
{
    uint32_t phys = pmm_alloc_frame();
    if (phys == 0)
        return NULL;

    void* virt = phys_to_virt(phys);
    zero_page(virt);
    return virt;
}

void vmm_free_page(void* page)
{
    if (page == NULL)
        return;

    uint32_t phys = virt_to_phys_addr(page);
    pmm_free_frame(phys);
}

void vmm_map_page(uint32_t* pagedir, uint32_t vaddr, uint32_t phys, uint32_t flags)
{
    /* page-align inputs */
    uint32_t va = vaddr & PAGE_FRAME_MASK;
    uint32_t pa = phys & PAGE_FRAME_MASK;

    uint32_t* pte = get_pte_for(pagedir, va, 1);
    if (!pte)
        return; /* allocation failed */

    *pte = (pa & PAGE_FRAME_MASK) | (flags & 0xFFF) | PAGE_PRESENT;

    /* Invalidate the single page in TLB */
    asm volatile ("invlpg (%0)" : : "r" (va) : "memory");
}

void vmm_unmap_page(uint32_t* pagedir, uint32_t vaddr)
{
    uint32_t va = vaddr & PAGE_FRAME_MASK;
    uint32_t* pte = get_pte_for(pagedir, va, 0);
    if (!pte)
        return;

    *pte = 0;
    asm volatile ("invlpg (%0)" : : "r" (va) : "memory");
}

uint32_t vmm_virt_to_phys(void* vaddr)
{
    uint32_t va = (uint32_t)(uintptr_t)vaddr;
    uint32_t va_page = va & PAGE_FRAME_MASK;

    uint32_t* pte = get_pte_for(kernel_page_directory, va_page, 0);
    if (!pte)
        return 0;

    uint32_t entry = *pte;
    if (!(entry & PAGE_PRESENT))
        return 0;

    uint32_t phys_page = entry & PAGE_FRAME_MASK;
    uint32_t offset = va & ~PAGE_FRAME_MASK;
    return phys_page + offset;
}
