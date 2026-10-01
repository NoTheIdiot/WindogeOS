bits 64

%include "headers/user/dogeio.inc"

%assign SYS_EXIT 60

section .text
global _start
_start:

    mov rax, SPECIAL
    syscall

    push r11
    push rcx

    mov rax, DOGEIO_PRINT
    mov rdi, text
    syscall

    pop rcx
    pop r11

    mov rax, SYS_EXIT
    syscall

section .data
align 16
text: db "WindogeOS", 0
