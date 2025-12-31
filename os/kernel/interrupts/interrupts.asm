global irq0
global irq1

extern irq_handler

irq0:
    pusha
    push 0
    call irq_handler
    add esp, 4
    popa
    iretd

irq1:
    pusha
    push 1
    call irq_handler
    add esp, 4
    popa
    iretd
