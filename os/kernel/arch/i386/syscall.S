; kernel/arch/i386/syscall.S
BITS 32
global syscall_handler
extern syscall_dispatch

section .text
syscall_handler:
    cli

    push ebx        ; arg1 (pointer la string)
    push eax        ; syscall number

    call syscall_dispatch

    add esp, 8

    sti
    iretd
