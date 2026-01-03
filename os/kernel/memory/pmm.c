#include "pmm.h"
#include <stddef.h>
#include <stdint.h>

/* Multiboot structs (minimal) */
typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_t;

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} multiboot_info_t;

/* kernel symbol (defined in linker.ld) */
extern uint32_t kernel_end;

/* bitmap data */
static uint32_t* bitmap;
static uint32_t  total_frames;
static uint32_t  used_frames;

/* align helper – SINGLE VERSION */
static inline uintptr_t align_up_uintptr(uintptr_t v)
{
    return (v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/* bitmap helpers */
static inline void bitmap_set(uint32_t frame)
{
    bitmap[frame / 32] |= (1u << (frame % 32));
}

static inline void bitmap_clear(uint32_t frame)
{
    bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static inline int bitmap_test(uint32_t frame)
{
    return (bitmap[frame / 32] & (1u << (frame % 32))) != 0;
}

/* API */
void pmm_init(void* mbi_ptr)
{
    multiboot_info_t* mbi = (multiboot_info_t*)mbi_ptr;

    /* calculate total RAM from mem_upper (KB) */
    uint32_t mem_kb = mbi->mem_upper + 1024; /* mem_upper is KB above 1MB */
    uint32_t mem_bytes = mem_kb * 1024u;

    total_frames = mem_bytes / PAGE_SIZE;

    /* bitmap size in bytes, aligned up to PAGE_SIZE */
    uintptr_t bitmap_bytes = (uintptr_t)((total_frames + 7) / 8);
    bitmap_bytes = align_up_uintptr(bitmap_bytes);

    /* place bitmap right after kernel_end, page-aligned */
    bitmap = (uint32_t*)align_up_uintptr((uintptr_t)&kernel_end);

    /* 1. mark all frames as USED initially (set all bits = 1) */
    uint32_t bitmap_dwords = (uint32_t)(bitmap_bytes / 4);
    for (uint32_t i = 0; i < bitmap_dwords; i++)
        bitmap[i] = 0xFFFFFFFFu;

    used_frames = total_frames;

    /* 2. parse multiboot memory map and free AVAILABLE (type == 1) frames */
    multiboot_mmap_t* mmap = (multiboot_mmap_t*)(uintptr_t)mbi->mmap_addr;
    uintptr_t mmap_end = (uintptr_t)mbi->mmap_addr + (uintptr_t)mbi->mmap_length;

    while ((uintptr_t)mmap < mmap_end) {

        if (mmap->type == 1) { /* available RAM */
            uintptr_t start = (uintptr_t)mmap->addr;
            uintptr_t end   = (uintptr_t)(mmap->addr + mmap->len);

            /* iterate page-by-page; compute frame by shifting (avoid 64-bit div) */
            for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
                uint32_t frame = (uint32_t)(addr >> 12); /* addr / 4096 */
                if (frame < total_frames) {
                    if (bitmap_test(frame)) {
                        bitmap_clear(frame);
                        used_frames--;
                    }
                }
            }
        }

        /* advance to next mmap entry (mmap->size does not include the size field itself) */
        mmap = (multiboot_mmap_t*)((uintptr_t)mmap + mmap->size + sizeof(uint32_t));
    }

    /* 3. reserve kernel frames (0 .. kernel_end_frame) */
    uint32_t kernel_end_f = (uint32_t)(((uintptr_t)&kernel_end) >> 12);
    for (uint32_t f = 0; f <= kernel_end_f; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
        }
    }

    /* 4. reserve frames used by the bitmap itself */
    uintptr_t bitmap_start = (uintptr_t)bitmap;
    uintptr_t bitmap_end   = bitmap_start + bitmap_bytes;

    for (uintptr_t addr = bitmap_start; addr < bitmap_end; addr += PAGE_SIZE) {
        uint32_t f = (uint32_t)(addr >> 12);
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
        }
    }
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t f = 0; f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
            return f * PAGE_SIZE;
        }
    }
    return 0; /* out of memory */
}

void pmm_free_frame(uint32_t phys_addr)
{
    uint32_t frame = phys_addr / PAGE_SIZE;
    if (frame >= total_frames) return;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        used_frames--;
    }
}

uint32_t pmm_total_frames(void)
{
    return total_frames;
}

uint32_t pmm_used_frames(void)
{
    return used_frames;
}

/* Inspect helper exported to commands/debugging */
int pmm_is_frame_used(uint32_t frame)
{
    if (frame >= total_frames) return -1;
    return (bitmap[frame / 32] & (1u << (frame % 32))) ? 1 : 0;
}

