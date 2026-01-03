#include "user_mem.h"
#include "vmm.h"
#include "paging.h"    /* pentru PAGE_* defines */
#include <stdint.h>

void user_map_page(address_space_t* as, uint32_t vaddr, uint32_t paddr)
{
    if (!as)
        return;

    /* as->page_directory trebuie să fie un pointer kernel-virtual la PD (uint32_t*).
     * Dacă folosești create_user_pagetable() care returnează adresa fizică,
     * adaptează aici (folosește vmm_virt_to_phys/phys_to_virt sau schimbă semnătura).
     */
    if (!as->page_directory)
        return;

    /* vmm_map_page va aloca tabelul de pagini secundar dacă e necesar
     * și va adăuga PAGE_PRESENT automat (impl. în vmm.c). Așadar trimitem
     * permisiunile de scriere + user.
     */
    vmm_map_page(as->page_directory, vaddr, paddr, PAGE_RW | PAGE_USER);
}
