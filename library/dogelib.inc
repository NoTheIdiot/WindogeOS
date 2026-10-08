%include "internal/syscalls.inc"

%macro shell 1
    mov rax, SHELL
    mov rdi, %1
    syscall
%endmacro

%macro exec 1
    mov rax, EXEC
    mov rdi, %1
    syscall
%endmacro