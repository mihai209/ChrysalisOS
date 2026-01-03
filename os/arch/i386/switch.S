    .text
    .global switch_to
    .type switch_to, @function
/* void switch_to(task_t *prev, task_t *next)
   cdecl: prev at 8(%esp), next at 12(%esp) */
switch_to:
    movl 8(%esp), %eax    /* prev */
    movl 12(%esp), %edx   /* next */

    /* save current stack pointer into prev->esp (offset 0) */
    movl %esp, (%eax)

    /* load next->esp into esp */
    movl (%edx), %esp

    /* return to the saved EIP on new stack */
    ret
