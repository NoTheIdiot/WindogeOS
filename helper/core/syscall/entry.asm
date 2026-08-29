[bits 64]

global syscall_entry
extern c_syscall_handler

section .text

syscall_entry:
    mov rdx, rsp
    swapgs
    mov [gs:8], rdx        ; save user RSP
    mov rsp, qword [gs:0]  ; switch to kernel stack

    push r15
    push r14
    push r13
    push rbx
    push rbp
    push r11               ; user RFLAGS
    push rcx               ; user RIP

    ; map syscall args to System V ABI
    mov r13, rdi
    mov r14, rsi
    mov r15, rdx
    mov rdi, rax
    mov rsi, r13
    mov rdx, r14
    mov rcx, r15
    mov r8,  r10

    call c_syscall_handler

    mov r12, rax           ; save return value in r12 temporarily

    pop rcx
    pop r11
    pop rbp
    pop rbx
    pop r13
    pop r14
    pop r15                 ; restore the dang regs (still on kernel stack)

    ; pop user RFLAGS and RIP into r11 and rcx (they were pushed in that order)
    ; (already popped above in reverse order)

    mov rdx, qword [gs:8]  ; grab saved user rsp from GS:8 while GS still points to kernel GS data
    swapgs                 ; restore user GS base
    mov rsp, rdx           ; switch to user stack

    mov rax, r12           ; put the return value back to rax

    ; sysretq will use rcx (RIP) and r11 (RFLAGS)
    sysretq
