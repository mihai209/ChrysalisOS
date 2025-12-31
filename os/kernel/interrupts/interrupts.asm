global irq0
global irq1
global idt_load

extern irq_handler

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

irq0:
    pusha
    call irq_handler
    popa
    iretd

irq1:
    pusha
    call irq_handler
    popa
    iretd
