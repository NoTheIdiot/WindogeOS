bits 64

global to_userland_ring3
global to_userland_ring3_args

section .text

to_userland_ring3:
    xor edx, edx
    xor ecx, ecx

to_userland_ring3_args:
    ; RDI = entry
    ; RSI = stack
    ; RDX = arg0
    ; RCX = arg1

    mov r8,  rdi
    mov r9,  rsi
    mov r10, rdx
    mov r11, rcx

    cli

    ; User GS base = 0
    mov ecx, 0xC0000101
    xor eax, eax
    xor edx, edx
    wrmsr

    ; Kernel GS base = 0
    mov ecx, 0xC0000102
    xor eax, eax
    xor edx, edx
    wrmsr

    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; SS
    push qword 0x1B

    ; RSP
    push r9

    ; RFLAGS
    push qword 0x202

    ; CS
    push qword 0x23

    ; RIP
    push r8

    ; SysV arguments
    mov rdi, r10
    mov rsi, r11

    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    iretq