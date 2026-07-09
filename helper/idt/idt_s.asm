[bits 64]               ; start at 64 bits
global irq_stub         ; global irq_stub because C is going to be translated into assembly anyway
extern irq_handler      ; get the handler

irq_stub:
    ; save all general registers
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; call the phooking handler
    cld                 ; clear direction flag
    call irq_handler

    ; restore everything
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    iretq