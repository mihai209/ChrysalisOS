; kernel/arch/i386/tss_flush.S  (NASM syntax, elf32)
BITS 32
global tss_flush
section .text

tss_flush:
    mov ax, 0x28     ; selector TSS = index 5 << 3
    ltr ax
    ret
