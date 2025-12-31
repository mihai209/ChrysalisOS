section .multiboot
align 4
    dd 0x1BADB002        ; magic number
    dd 0x00              ; flags
    dd -(0x1BADB002)     ; checksum

section .text
global _start
extern kernel_main

_start:
    cli
    call kernel_main
.hang:
    hlt
    jmp .hang
