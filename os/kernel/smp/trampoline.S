[BITS 16]
[ORG 0x7000]

global trampoline_start

trampoline_start:
    cli

    ; Load GDT
    lgdt [gdt_desc]

    ; Enter 32-bit Protected Mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to flush pipeline and load CS with 32-bit code segment
    jmp 0x08:protected_mode

[BITS 32]
protected_mode:
    ; Setup data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Load stack pointer (patched by BSP) - Physical address? No, virtual.
    ; But we are not paging yet. Stack is in BSS/Data, mapped at 0xC0...
    ; We need paging enabled to use the stack if it's in high memory.
    mov esp, [ap_stack_ptr]

    ; Enable Paging to see the kernel
    ; Load CR3 with the page directory (patched by BSP)
    mov eax, [ap_cr3_ptr]
    mov cr3, eax

    mov eax, cr0
    or eax, 0x80000000 ; Set PG bit
    mov cr0, eax

    ; Jump to AP C entry (patched by BSP)
    jmp [ap_entry_ptr]

    ; Should not return from ap_main
ap_hang:
    hlt
    jmp ap_hang

; This needs to be patched by the BSP before sending SIPI.
gdt_desc:
    dw 0 ; limit (filled by C code)
    dd 0 ; base (filled by C code)

ap_stack_ptr:
    dd 0 ; stack pointer (filled by C code)

ap_cr3_ptr:
    dd 0 ; CR3 / Page Directory (filled by C code)

ap_entry_ptr:
    dd 0 ; entry point (filled by C code)