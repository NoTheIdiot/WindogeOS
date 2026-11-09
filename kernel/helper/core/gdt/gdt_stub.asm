; tell that it's 64 bit
bits 64
global gdt_flush

section .text
gdt_flush:
    lgdt [rdi]
    
    ; flush the gdt registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push qword 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ; continue setting the address
    mov ax, 0x28
    ltr ax
    ret