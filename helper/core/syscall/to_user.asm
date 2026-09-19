bits 64

global to_userland_ring3

; c function:
; void to_userland_ring3(uint64_t user_rip, uint64_t user_rsp);
; args:  
; rdi = user_rip
; rsi = user_rsp

section .text
to_userland_ring3:
    ; Disable interrupts during stack frame construction
    cli

    ; Load Ring 3 data segment selectors (0x18 | RPL 3 = 0x1B)
    mov ax, 0x1b
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x1b     ; SS  = User Data (0x18 | 3)
    push rsi            ; RSP = User RSP
    push qword 0x202    ; RFLAGS = IF enabled (bit 9) + reserved bit 1
    push qword 0x23     ; CS  = User Code (0x20 | 3)
    push rdi            ; RIP = User RIP

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

    ; restrict your freedom :)
    iretq