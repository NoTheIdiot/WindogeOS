bits 64

global system_run_elf
global syscall_exit_to_launcher

extern run_elf

section .bss
saved_rsp:    resq 1
saved_ret:    resq 1

saved_rbx:    resq 1
saved_rbp:    resq 1
saved_r12:    resq 1
saved_r13:    resq 1
saved_r14:    resq 1
saved_r15:    resq 1

section .text

system_run_elf:
    mov [rel saved_rbx], rbx
    mov [rel saved_rbp], rbp
    mov [rel saved_r12], r12
    mov [rel saved_r13], r13
    mov [rel saved_r14], r14
    mov [rel saved_r15], r15

    mov [rel saved_rsp], rsp

    mov rax, [rsp]
    mov [rel saved_ret], rax

    sub rsp, 8
    call run_elf
    add rsp, 8

    ret

syscall_exit_to_launcher:
    mov rbx, [rel saved_rbx]
    mov rbp, [rel saved_rbp]
    mov r12, [rel saved_r12]
    mov r13, [rel saved_r13]
    mov r14, [rel saved_r14]
    mov r15, [rel saved_r15]

    mov rsp, [rel saved_rsp]

    xor eax, eax

    mov rax, [rel saved_ret]
    jmp rax