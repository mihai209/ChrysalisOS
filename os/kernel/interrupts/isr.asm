[BITS 32]

global irq0
global irq1

extern irq_handler

; -----------------------
; IRQ 0 (timer)
; -----------------------
irq0:
    cli
    push byte 0          ; fake error code
    push byte 32         ; IRQ0 = interrupt 32
    jmp irq_common_stub

; -----------------------
; IRQ 1 (keyboard)
; -----------------------
irq1:
    cli
    push byte 0
    push byte 33         ; IRQ1 = interrupt 33
    jmp irq_common_stub

; -----------------------
; common handler
; -----------------------
irq_common_stub:
    cld                  ; GCC code expects Direction Flag to be 0
    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10         ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp             ; pass struct registers*
    call irq_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds

    popa
    add esp, 8           ; remove int_no + err_code
    sti
    iretd
