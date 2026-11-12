bits 64

global to_userland_ring3
global to_userland_ring3_args

section .text
to_userland_ring3:
    xor edx, edx
    xor ecx, ecx

to_userland_ring3_args:
    mov r8, rdi
    mov r9, rsi
    mov r10, rdx
    mov r11, rcx
    cli

    mov ax, 0x1b
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x1b
    push r9
    push qword 0x202
    push qword 0x23
    push r8

    mov rdi, r10
    mov rsi, r11
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    iretq