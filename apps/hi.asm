bits 64

%include "headers/user/dogeio.inc"

section .text
global _start
_start:

    ; it worked.
    mov rax, SPECIAL
    syscall

section .data
filename: db "hi", 0
message: db "hello world", 10, 0