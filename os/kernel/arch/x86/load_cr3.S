/* arch/x86/load_cr3.S
 * Assembly helper to load CR3 on ia32 (-m32)
 * Signature: void load_cr3(uint32_t pd_phys);
 * Implementation expects cdecl calling convention: argument at 4(%esp).
 */

    .text
    .global load_cr3
    .type load_cr3, @function
load_cr3:
    /* get argument from stack into eax */
    mov 4(%esp), %eax
    /* load CR3 from eax */
    mov %eax, %cr3
    ret
