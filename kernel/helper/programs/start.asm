bits 64

global system_run_elf
global syscall_exit_to_launcher

extern run_elf

section .bss
saved_rsp: resq 1
saved_rbx: resq 1
saved_rbp: resq 1
saved_r12: resq 1
saved_r13: resq 1
saved_r14: resq 1
saved_r15: resq 1

section .text

system_run_elf:
    mov [rel saved_rbx], rbx
    mov [rel saved_rbp], rbp
    mov [rel saved_r12], r12
    mov [rel saved_r13], r13
    mov [rel saved_r14], r14
    mov [rel saved_r15], r15
    mov [rel saved_rsp], rsp

    call run_elf
    ret

syscall_exit_to_launcher:
    cli
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, [rel saved_rsp]
    mov rbx, [rel saved_rbx]
    mov rbp, [rel saved_rbp]
    mov r12, [rel saved_r12]
    mov r13, [rel saved_r13]
    mov r14, [rel saved_r14]
    mov r15, [rel saved_r15]

    sti
    xor eax, eax
    ret