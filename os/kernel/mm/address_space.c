#include "address_space.h"
#include "paging.h"
#include "../memory/pmm.h"
#include "vmm.h"

#include <stddef.h>   /* size_t */
#include <stdint.h>

extern uint32_t* kernel_page_directory;

/* If you have heap headers, you can replace the forward-decls with an include:
 * #include "../memory/heap.h"
 * but forward-declares are safer if header path is different.
 */
void* kmalloc(size_t size);
void kfree(void* ptr);

address_space_t* address_space_create()
{
    address_space_t* as = (address_space_t*)kmalloc(sizeof(address_space_t));
    if (!as)
        return NULL;

    /* alocă page directory (kernel-virtual pointer) */
    as->page_directory = (uint32_t*)vmm_alloc_page();
    if (!as->page_directory) {
        kfree(as);
        return NULL;
    }

    /* copiază kernel mappings (3GB–4GB) */
    for (int i = 768; i < 1024; i++)
        as->page_directory[i] = kernel_page_directory[i];

    /* user part = gol */
    for (int i = 0; i < 768; i++)
        as->page_directory[i] = 0;

    return as;
}

void address_space_switch(address_space_t* as)
{
    if (!as || !as->page_directory)
        return;

    /* Convert kernel-virtual page-directory pointer to physical before loading CR3 */
    uint32_t pd_phys = vmm_virt_to_phys(as->page_directory);
    paging_load_directory(pd_phys);
}

void address_space_destroy(address_space_t* as)
{
    if (!as) return;

    /* free page-directory page */
    if (as->page_directory)
        vmm_free_page(as->page_directory);

    kfree(as);
}
