bits 64
org 0x00400000

%define SYS_EXIT 0
%define FS_WRITE 2

section .text
global _start
_start:
    ; fs write
    mov rax, FS_WRITE
    lea rdi, [rel filename]
    lea rsi, [rel message]
    syscall

    mov rax, SYS_EXIT
    mov rdi, 0
    syscall

section .data
filename: db "hi", 0
message: db "hello world", 10, 0