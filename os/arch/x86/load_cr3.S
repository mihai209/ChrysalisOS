    .text
    .global load_cr3
    .type load_cr3, @function
load_cr3:
    /* cdecl: argument at 4(%esp) */
    mov 4(%esp), %eax
    /* load CR3 from eax */
    mov %eax, %cr3
    ret
