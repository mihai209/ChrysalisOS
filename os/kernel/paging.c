/* kernel/paging.c */
#include "paging.h"
#include "mm/paging.h" /* for KERNEL_BASE */
#include "memory/pmm.h"
#include "string.h"
#include <stdint.h>

/* Import the global directory from kernel/mm/paging.c */
extern uint32_t* kernel_page_directory;

/* Constants */
#define KERNEL_PHYS_BASE    0x00000000 /* FIX: Map from 0 to align with phys_to_virt */
#define PAGE_SIZE           4096

void paging_init(uint32_t ram_size_mb) {
    (void)ram_size_mb;
    
    /* Allocate a frame for the kernel page directory */
    uint32_t pd_phys = pmm_alloc_frame();
    
    /* Temporarily access it using physical address */
    kernel_page_directory = (uint32_t*)pd_phys;
    
    /* Clear it */
    memset(kernel_page_directory, 0, PAGE_SIZE);
}

void paging_map_kernel_higher_half(void) {
    /* Map physical memory starting from 0 to KERNEL_BASE (0xC0000000) */
    /* We map 32MB to cover kernel code, data, and some initial heap */
    
    uint32_t virt_start = KERNEL_BASE;
    uint32_t phys_start = KERNEL_PHYS_BASE;
    uint32_t map_size   = 64 * 1024 * 1024; /* 64 MB to cover 32MB heap + kernel + bitmap */
    
    /* 1. Identity map the first 32MB (covers kernel low-half code, VGA, etc.)
     * This is CRITICAL: without this, enabling paging (CR0.PG=1) causes an
     * immediate Triple Fault because the currently executing code (EIP) is
     * at a low physical address which wouldn't be mapped.
     */
    for (uint32_t offset = 0; offset < map_size; offset += PAGE_SIZE) {
        uint32_t vaddr = phys_start + offset; /* Identity: virt = phys */
        uint32_t paddr = phys_start + offset;
        
        uint32_t pd_index = vaddr >> 22;
        uint32_t pt_index = (vaddr >> 12) & 0x3FF;
        
        if (!(kernel_page_directory[pd_index] & PAGE_PRESENT)) {
            uint32_t pt_phys = pmm_alloc_frame();
            uint32_t* pt = (uint32_t*)pt_phys;
            memset(pt, 0, PAGE_SIZE);
            kernel_page_directory[pd_index] = pt_phys | PAGE_PRESENT | PAGE_RW;
        }
        
        uint32_t pt_phys = kernel_page_directory[pd_index] & ~0xFFF;
        uint32_t* pt = (uint32_t*)pt_phys;
        pt[pt_index] = paddr | PAGE_PRESENT | PAGE_RW;
    }

    /* 2. Map higher half (0xC0000000 -> 0x00000000) */
    for (uint32_t offset = 0; offset < map_size; offset += PAGE_SIZE) {
        uint32_t vaddr = virt_start + offset;
        uint32_t paddr = phys_start + offset;
        
        uint32_t pd_index = vaddr >> 22;
        uint32_t pt_index = (vaddr >> 12) & 0x3FF;
        
        /* Check if Page Table exists */
        if (!(kernel_page_directory[pd_index] & PAGE_PRESENT)) {
            uint32_t pt_phys = pmm_alloc_frame();
            uint32_t* pt = (uint32_t*)pt_phys; /* Access as phys for now */
            memset(pt, 0, PAGE_SIZE);
            
            kernel_page_directory[pd_index] = pt_phys | PAGE_PRESENT | PAGE_RW;
        }
        
        /* Get Page Table */
        uint32_t pt_phys = kernel_page_directory[pd_index] & ~0xFFF;
        uint32_t* pt = (uint32_t*)pt_phys;
        
        /* Set Page Table Entry */
        pt[pt_index] = paddr | PAGE_PRESENT | PAGE_RW;
    }
    
    /* Recursive mapping for the PD itself at the last entry (1023) */
    kernel_page_directory[1023] = (uint32_t)kernel_page_directory | PAGE_PRESENT | PAGE_RW;
    
    /* Load CR3 */
    asm volatile("mov %0, %%cr3" :: "r"(kernel_page_directory));
    
    /* Enable Paging */
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; /* Set PG bit */
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
    
    /* Update kernel_page_directory to virtual address */
    kernel_page_directory = (uint32_t*)((uintptr_t)kernel_page_directory + KERNEL_BASE);
}

int paging_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    if (!(kernel_page_directory[pd_index] & PAGE_PRESENT)) {
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) return -1;
        
        uint32_t* pt_virt = (uint32_t*)(pt_phys + KERNEL_BASE);
        memset(pt_virt, 0, PAGE_SIZE);
        
        uint32_t pd_flags = PAGE_PRESENT | PAGE_RW;
        if (flags & PAGE_USER) pd_flags |= PAGE_USER;
        
        kernel_page_directory[pd_index] = pt_phys | pd_flags;
    }

    uint32_t pt_phys = kernel_page_directory[pd_index] & ~0xFFF;
    uint32_t* pt_virt = (uint32_t*)(pt_phys + KERNEL_BASE);
    
    pt_virt[pt_index] = (physical_addr & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
    asm volatile("invlpg (%0)" :: "r" (virtual_addr) : "memory");
    return 0;
}

void paging_unmap_page(uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    if (kernel_page_directory[pd_index] & PAGE_PRESENT) {
        uint32_t pt_phys = kernel_page_directory[pd_index] & ~0xFFF;
        uint32_t* pt_virt = (uint32_t*)(pt_phys + KERNEL_BASE);
        pt_virt[pt_index] = 0;
        asm volatile("invlpg (%0)" :: "r" (virtual_addr) : "memory");
    }
}

uint32_t paging_get_page_directory_phys(void) {
    return (uint32_t)((uintptr_t)kernel_page_directory - KERNEL_BASE);
}