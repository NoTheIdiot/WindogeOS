bits 64
global gdt_flush

section .text

gdt_flush:
    lgdt [rdi]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    mov ax, 0x28
    ltr ax
    ret