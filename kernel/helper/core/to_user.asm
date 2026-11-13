bits 64

global to_userland_ring3
global to_userland_ring3_args
global syscall_exit_to_launcher

extern kernel_program_launcher_rsp

section .text
to_userland_ring3:
    xor edx, edx
    xor ecx, ecx

to_userland_ring3_args:
    cli

    push rdi
    push rsi

    mov ax, 0x1b
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    pop rsi
    pop rdi

    push qword 0x1b
    push rsi
    push qword 0x202
    push qword 0x23
    push rdi

    mov rdi, rdx
    mov rsi, rcx

    xor rbx, rbx
    xor rbp, rbp
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    iretq

syscall_exit_to_launcher:
    cli
    mov rax, rdi
    mov rsp, [rel kernel_program_launcher_rsp]
    mov qword [rel kernel_program_launcher_rsp], 0

    mov dx, 0x10
    mov ds, dx
    mov es, dx

    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
