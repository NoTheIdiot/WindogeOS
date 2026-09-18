bits 64

%include "headers/user/dogeio.inc"

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