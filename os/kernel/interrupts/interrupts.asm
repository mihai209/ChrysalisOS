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
    push byte 32         ; interrupt number
    jmp irq_common_stub

; -----------------------
; IRQ 1 (keyboard)
; -----------------------
irq1:
    cli
    push byte 0
    push byte 33
    jmp irq_common_stub

; -----------------------
; Common IRQ handler
; -----------------------
irq_common_stub:
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

    push esp             ; pointer to registers_t
    call irq_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds

    popa
    add esp, 8           ; pop int_no + err_code
    sti
    iretd
