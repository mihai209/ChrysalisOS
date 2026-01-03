#include "paging.h"
#include <stdint.h>
#include <stddef.h>

/* Optional: if you want to print text during init (make sure terminal_init already called) */
extern void terminal_printf(const char*, ...);

/* Constants */
#define PAGE_SIZE           0x1000U
#define ENTRIES_PER_TABLE   1024U
#define TABLE_SIZE_BYTES    (ENTRIES_PER_TABLE * 4U) /* 4096 bytes */
#define ONE_DIR_ENTRY_BYTES 4U
#define PD_INDEX(x)         (((x) >> 22) & 0x3FF)
#define PT_INDEX(x)         (((x) >> 12) & 0x3FF)
#define ALIGN_4K __attribute__((aligned(4096)))

/* Config */
#define DEFAULT_IDENTITY_MAP_MB PAGING_120_MB
#define MAX_PAGE_TABLES 64U          /* total page-tables we keep statically */

/* Page directory (aligned) */
static uint32_t page_directory[ENTRIES_PER_TABLE] ALIGN_4K;

/* Pool of page tables (aligned). We'll use first N for identity mapping then allocate more on demand. */
static uint32_t page_tables[MAX_PAGE_TABLES][ENTRIES_PER_TABLE] ALIGN_4K;
static uint32_t next_free_table = 0;

/* Helper: simple check for alignment */
static inline int is_page_aligned(uint32_t v) {
    return (v & (PAGE_SIZE - 1)) == 0;
}

/* Returns physical address of a page-table pointer (before paging enabled virtual==physical). */
static inline uint32_t phys_of(void* p) {
    return (uint32_t)p;
}

/* Enable paging: write CR3 and set CR0.PG bit */
static void enable_paging_internal(uint32_t pd_phys) {
    /* load CR3 */
    asm volatile("mov %0, %%cr3" :: "r"(pd_phys));
    /* set PG bit in CR0 */
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U; /* set PG */
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

/* Create a new page-table from pool; returns index or -1 on failure */
static int allocate_page_table(void) {
    if (next_free_table >= MAX_PAGE_TABLES) return -1;
    /* Zero table for cleanliness */
    uint32_t idx = next_free_table++;
    for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) page_tables[idx][i] = 0;
    return (int)idx;
}

/* Accept only the predefined MB profiles (20,40,60,80,100,120) */
static int paging_is_valid_mb(uint32_t mb) {
    switch (mb) {
        case 20:
        case 40:
        case 60:
        case 80:
        case 100:
        case 120:
            return 1;
        default:
            return 0;
    }
}

void paging_init(uint32_t identity_map_mb) {
    /* if 0 => use default, if invalid => fallback default */
    if (identity_map_mb == 0) identity_map_mb = DEFAULT_IDENTITY_MAP_MB;
    if (!paging_is_valid_mb(identity_map_mb)) {
        if (terminal_printf)
            terminal_printf("paging: invalid profile %u MB, falling back to %u MB\n", identity_map_mb, DEFAULT_IDENTITY_MAP_MB);
        identity_map_mb = DEFAULT_IDENTITY_MAP_MB;
    }

    /* Zero page directory */
    for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) page_directory[i] = 0;

    /* Reset pool */
    next_free_table = 0;
    for (uint32_t t = 0; t < MAX_PAGE_TABLES; ++t) {
        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) page_tables[t][i] = 0;
    }

    /* How many 4MB chunks we need to cover identity_map_mb */
    uint32_t mb = identity_map_mb;
    uint32_t four_mb_chunks = (mb + 3) / 4; /* ceil(mb/4) */

    uint32_t flags = PAGE_PRESENT | PAGE_RW; /* kernel RW present */

    uint32_t used_tables = 0;
    for (uint32_t chunk = 0; chunk < four_mb_chunks; ++chunk) {
        int table_idx = allocate_page_table();
        if (table_idx < 0) {
            /* allocation failure */
            if (terminal_printf) terminal_printf("paging: no free page-tables\n");
            return;
        }
        used_tables++;

        /* Set PDE to point to this page_table */
        uint32_t pde_val = phys_of(&page_tables[table_idx]) | flags;
        page_directory[chunk] = pde_val;

        /* Fill the page table: identity map 4MB chunk */
        uint32_t base_phys = chunk * 0x400000U; /* each page table maps 4MB */
        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) {
            uint32_t phys_addr = base_phys + (i * PAGE_SIZE);
            page_tables[table_idx][i] = phys_addr | flags;
        }
    }

    /* Optional debug */
    if (terminal_printf) terminal_printf("paging: identity-mapped %u MB using %u page-tables\n", mb, used_tables);

    /* Enable paging: pass physical address of page_directory (identity mapped so &page_directory is physical) */
    enable_paging_internal(phys_of(page_directory));

    if (terminal_printf) terminal_printf("paging: enabled (CR3 set)\n");
}

/* Map a single page (will allocate a page-table if missing). Returns 0 on success, non-zero on fail. */
int paging_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    if (!is_page_aligned(virtual_addr) || !is_page_aligned(physical_addr)) return -1;

    uint32_t pdx = PD_INDEX(virtual_addr);
    uint32_t ptx = PT_INDEX(virtual_addr);

    uint32_t pde = page_directory[pdx];
    if ((pde & PAGE_PRESENT) == 0) {
        int idx = allocate_page_table();
        if (idx < 0) return -2; /* no free page table */
        page_directory[pdx] = phys_of(&page_tables[idx]) | (PAGE_PRESENT | PAGE_RW);
        pde = page_directory[pdx];
    }

    /* We assume page_directory entries are physical addresses to page_tables (identity mapped) */
    uint32_t *pt = (uint32_t*)(pde & 0xFFFFF000U);
    pt[ptx] = physical_addr | (flags & 0xFFFU);
    /* invalidate TLB for that page */
    asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");

    return 0;
}

void paging_unmap_page(uint32_t virtual_addr) {
    if (!is_page_aligned(virtual_addr)) return;

    uint32_t pdx = PD_INDEX(virtual_addr);
    uint32_t ptx = PT_INDEX(virtual_addr);

    uint32_t pde = page_directory[pdx];
    if ((pde & PAGE_PRESENT) == 0) return;

    uint32_t *pt = (uint32_t*)(pde & 0xFFFFF000U);
    pt[ptx] = 0;
    asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}

uint32_t paging_get_page_directory_phys(void) {
    return phys_of(page_directory);
}
