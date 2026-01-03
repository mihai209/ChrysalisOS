#pragma once
#include <stdint.h>
#include "address_space.h"

/* Top of the user stack (page-aligned). Ajustează dacă vrei alt offset. */
#define USER_STACK_TOP 0xBFFFE000u

/* Map a user virtual page in the given address space.
 * - as     : adresă la address_space_t (trebuie să aibă page_directory valid)
 * - vaddr  : virtual address (page-aligned)
 * - paddr  : physical frame (page-aligned)
 *
 * Notă: funcția folosește vmm_map_page() intern, care setează PAGE_PRESENT.
 */
void user_map_page(address_space_t* as, uint32_t vaddr, uint32_t paddr);
