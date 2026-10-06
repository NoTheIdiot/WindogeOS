bits 64

%include "headers/user/dogeio.inc"

section .text
global _start
_start:

    mov rax, create_file
    mov rdi, filename
    syscall
    
    push r11
    push rcx

    mov rax, println
    mov rdi, text
    syscall

    push r11
    push rcx

    mov rax, sys_exit
    mov rdi, 1
    syscall

section .data
align 16
filename: db "file.txt", 0
text: db "file created", 0