bits 64
global core_to_user

section .text
core_to_user:
    cli

    push qword 0x23        
    push rdx               
    push qword 0x202       
    push qword 0x1B        
    push rsi               

    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rbp, rbp
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    mov ax, 0x23
    mov ds, ax
    mov es, ax

    mov cr3, rdi           

    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi

    iretq
