/* kernel/mem/kmalloc.c
   Simple free-list kernel heap allocator with coalescing.
   - first-fit
   - split_block support
   - basic kmalloc_aligned with stored raw-pointer + magic so kfree can recover
   - no locking (disable interrupts or add spinlock externally if needed)
*/

#include "kmalloc.h"
#include <stddef.h>
#include <stdint.h>

/* If you want debug prints, define KMALLOC_DEBUG (and implement terminal_printf) */
#ifdef KMALLOC_DEBUG
/* only include if you have terminal_printf */
#include "../terminal.h"
#endif

/* Configuration */
#define ALIGNMENT 8u
#define ALIGN_UP(x, a) ((((uintptr_t)(x)) + ((uintptr_t)(a) - 1u)) & ~((uintptr_t)((a) - 1u)))

/* magic for aligned allocations */
#define KMALLOC_ALIGN_MAGIC 0xDEADBEEFu

/* block header placed immediately before the returned user pointer (for normal allocations) */
typedef struct kmem_block {
    size_t size;               /* usable size in bytes (not including header) */
    struct kmem_block* next;   /* next block in the free list (physical order) */
    uint8_t free;              /* 1 = free, 0 = allocated */
    /* padding to keep header size aligned */
} kmem_block_t;

/* Globals */
static kmem_block_t* free_list = NULL;
static uint8_t* heap_start = NULL;
static uint8_t* heap_end = NULL;

/* Forward decl */
static void split_block(kmem_block_t* block, size_t size);

/* Initialize heap region: start must be an address within kernel image after linking.
   size is the total bytes available for the heap region.
*/
void heap_init(void* start, size_t size) {
    if (!start || size <= sizeof(kmem_block_t) + ALIGNMENT) return;

    uintptr_t s = (uintptr_t)start;
    s = ALIGN_UP(s, ALIGNMENT);
    heap_start = (uint8_t*)s;
    heap_end = (uint8_t*)((uintptr_t)start + size);

    free_list = (kmem_block_t*)heap_start;
    free_list->size = (size_t)(heap_end - heap_start) - sizeof(kmem_block_t);
    free_list->next = NULL;
    free_list->free = 1;
}

/* Split a block into [block(size)] + [new_block(rest)] if big enough.
   'size' must be aligned already.
*/
static void split_block(kmem_block_t* block, size_t size) {
    if (!block) return;
    /* Need enough space for a new header + ALIGNMENT payload */
    if (block->size < size + sizeof(kmem_block_t) + ALIGNMENT) return;

    uint8_t* new_block_ptr = (uint8_t*)block + sizeof(kmem_block_t) + size;
    kmem_block_t* new_block = (kmem_block_t*)new_block_ptr;

    new_block->size = block->size - size - sizeof(kmem_block_t);
    new_block->next = block->next;
    new_block->free = 1;

    block->next = new_block;
    block->size = size;
}

/* kmalloc: allocate size bytes (aligned to ALIGNMENT) */
void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = (size_t)ALIGN_UP(size, ALIGNMENT);

    /* search first-fit */
    kmem_block_t* prev = NULL;
    kmem_block_t* cur = free_list;

    while (cur) {
        if (cur->free && cur->size >= size) {
            split_block(cur, size);
            cur->free = 0;
            void* userptr = (void*)((uint8_t*)cur + sizeof(kmem_block_t));
#ifdef KMALLOC_DEBUG
            /* cast for printing (kernel's printf signature may differ) */
            terminal_printf("[kmalloc] %u -> %p\n", (unsigned)size, userptr);
#endif
            return userptr;
        }
        prev = cur;
        cur = cur->next;
    }

    /* no suitable block */
    return NULL;
}

/* kfree: free the pointer previously returned by kmalloc or kmalloc_aligned */
void kfree(void* ptr) {
    if (!ptr) return;

    /* First: detect if this pointer was an aligned allocation wrapper.
       The scheme used by kmalloc_aligned stores:
         [magic: uint32_t][raw_ptr: uintptr_t] just before the aligned user pointer.
       We'll check for magic (and also ensure we don't read outside heap region).
    */
    uintptr_t min_magic_offset = sizeof(uint32_t) + sizeof(uintptr_t);
    uint8_t* p = (uint8_t*)ptr;

    if (heap_start != NULL && (p - min_magic_offset) >= heap_start) {
        uint8_t* magic_addr = p - min_magic_offset;
        uint32_t maybe_magic = *((uint32_t*)magic_addr);
        if (maybe_magic == KMALLOC_ALIGN_MAGIC) {
            /* recover original raw pointer */
            uintptr_t* raw_addr = (uintptr_t*)(p - sizeof(uintptr_t));
            void* raw_ptr = (void*)(*raw_addr);
            /* sanitize: ensure raw_ptr is inside heap */
            if ((uint8_t*)raw_ptr >= heap_start && (uint8_t*)raw_ptr < heap_end) {
                ptr = raw_ptr; /* now fall-through to free as normal */
            }
        }
    }

    /* header is immediately before ptr now */
    kmem_block_t* block = (kmem_block_t*)((uint8_t*)ptr - sizeof(kmem_block_t));
    block->free = 1;

    /* coalesce adjacent free blocks: iterate from start (simple) */
    kmem_block_t* cur = free_list;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            /* check adjacency in memory */
            uint8_t* cur_end = (uint8_t*)cur + sizeof(kmem_block_t) + cur->size;
            if (cur_end == (uint8_t*)cur->next) {
                /* merge */
                cur->size = cur->size + sizeof(kmem_block_t) + cur->next->size;
                cur->next = cur->next->next;
                /* continue without advancing cur to try to merge further */
                continue;
            }
        }
        cur = cur->next;
    }
}

/* kmalloc_aligned: returns pointer aligned to 'alignment' (power-of-two recommended)
   Implementation stores a small header (magic + raw_ptr) just before the returned pointer,
   so kfree can recover the original pointer and free correctly.
*/
void* kmalloc_aligned(size_t size, size_t alignment) {
    if (size == 0) return NULL;
    if (alignment <= ALIGNMENT) return kmalloc(size);

    /* alignment must be power-of-two ideally; we won't check here but recommended */
    /* We allocate extra space for:
       - alignment padding
       - a magic (uint32_t) + raw_ptr (uintptr_t) stored just before returned pointer
    */
    size_t extra = alignment + sizeof(uint32_t) + sizeof(uintptr_t);
    size_t real_size = size + extra;
    void* raw = kmalloc(real_size);
    if (!raw) return NULL;

    uintptr_t p = (uintptr_t)raw;
    uintptr_t offset = (uintptr_t)(sizeof(uint32_t) + sizeof(uintptr_t));
    uintptr_t aligned = ALIGN_UP(p + offset, alignment);

    /* store magic and raw pointer */
    uint8_t* magic_addr = (uint8_t*)(aligned - offset);
    uint32_t* magic = (uint32_t*)magic_addr;
    uintptr_t* raw_addr = (uintptr_t*)(aligned - sizeof(uintptr_t));

    *magic = KMALLOC_ALIGN_MAGIC;
    *raw_addr = (uintptr_t)raw;

    return (void*)aligned;
}
