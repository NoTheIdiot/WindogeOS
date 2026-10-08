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

%macro mkdir_recursive 1
    mov rax, MKDIR_RECURSIVE
    mov rdi, %1
    syscall
%endmacro

%macro get_time 1
    mov rax, GET_TIME
    mov rdi, %1
    syscall
%endmacro

%macro stat 2
    mov rax, STAT
    mov rdi, %1
    mov rsi, %2
    syscall
%endmacro