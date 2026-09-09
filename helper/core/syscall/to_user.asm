bits 64

global to_userland_ring3

; c function:
; void to_userland_ring3(uint64_t user_rip, uint64_t user_rsp);
; args:  
; rdi = user_rip
; rsi = user_rsp

section .text
to_userland_ring3:
    ; disable interrupts during the transition
    ; so the time doesn't corrupt the stack frame
    cli

    ; get data segment 
    ; 0x23 is user data, index 4 RPL 3
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; load the kernel's GS base for MSR_IA32_KERNEL_GS_BASE
    swapgs

    ; build iretq stack frame
    push qword 0x23
    push rsi

    ; 0x202 means intterupts yes (bit 9)
    push qword 0x202
    push rdi
\
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
    xor rdi, rdi
    xor rsi, rsi

    ; to ring 3
    iretq