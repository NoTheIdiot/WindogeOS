bits 64

section .text
_start:
    mov rax, 1
    syscall

    mov rax, 0
    syscall