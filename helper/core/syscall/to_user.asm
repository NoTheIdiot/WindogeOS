bits 64

global core_to_user

section .text

core_to_user:
    ; disable interrupts
    cli

    ; switch page tables
    mov cr3, rdi

    ; set up segment selectors
    ; 0x23 is user data selector (RPL 3)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; build the iretq stack frame (must be in correct order)
    ; iretq wants: [SS], [RSP], [RFALGS], [CS], [RIP] pushed into the stack
    ; in that order
    
    push 0x23                   ; SS (user data segment selector)
    push RDX                    ; RSP (user stack top passed in RSI, then move to RDX)
    push 0x202                  ; RFLAGS (INT enabled, reserved bit set)
    push 0x1B
    push rsi

    ; go straight to userspace
    iretq