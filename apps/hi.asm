bits 64

%include "headers/user/dogeio.inc"

%assign SYS_EXIT 60

section .text
global _start
_start:

    mov rax, special
    syscall

    push r11
    push rcx

    mov rax, special
    syscall

    push r11
    push rcx
    
    mov rax, special
    syscall

    push r11
    push rcx

    mov rax, println
    mov rdi, text
    syscall

    push r11
    push rcx

    mov rax, SYS_EXIT
    mov rdi, 0
    syscall

section .data
align 16
text: db "WindogeOS", 0
