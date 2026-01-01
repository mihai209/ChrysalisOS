; kernel/boot.asm
; Multiboot header + entry that forwards (magic, addr) to kernel_main
; NASM syntax

section .multiboot
align 4
    dd 0x1BADB002        ; magic number
    dd 0x00              ; flags (0 = minimal; you can set bits if you want)
    dd -(0x1BADB002)     ; checksum

section .text
global _start
extern kernel_main

_start:
    cli                 ; disable interrupts (safe start)

    ; GRUB (multiboot) sets:
    ;   EAX = magic
    ;   EBX = pointer to multiboot_info struct
    ;
    ; We must pass them to C kernel_main as cdecl arguments:
    ; push args right-to-left: first push EBX (addr), then EAX (magic),
    ; then call kernel_main which is declared as: void kernel_main(uint32_t magic, uint32_t addr);

    push    ebx         ; push multiboot_info pointer (addr)
    push    eax         ; push magic
    call    kernel_main

.hang:
    hlt
    jmp .hang
