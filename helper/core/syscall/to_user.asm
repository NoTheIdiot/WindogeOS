bits 64
global core_to_user
extern shell_rsp_backup
extern shell_rbp_backup
extern shell_rip_backup
extern shell_cr3_backup

section .text
core_to_user:
    cli

    ; Save the kernel state so a future exit can restore the original context.
    mov rax, cr3
    mov [rel shell_cr3_backup], rax
    mov [rel shell_rsp_backup], rsp
    mov [rel shell_rbp_backup], rbp
    lea rax, [rel .after_iret]
    mov [rel shell_rip_backup], rax

    mov cr3, rdi

    ; Switch to the user selectors.
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Build the IRETQ frame on the user stack itself.
    mov rsp, rdx
    push qword 0x23
    push qword 0x0000000000000000
    pushfq
    push qword 0x1B
    push rsi

    iretq

.after_iret:
    ret
