global enter_user_mode
enter_user_mode:
    cli

    mov ax, 0x23        ; user data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x23           ; SS
    push user_stack_top ; ESP
    pushf
    push 0x1B           ; CS
    push user_entry     ; EIP

    iret
