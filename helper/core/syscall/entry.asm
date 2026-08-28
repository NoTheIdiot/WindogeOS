[bits 64]

global syscall_entry
extern c_syscall_handler

section .text

syscall_entry:
    swapgs
    mov [gs:8], rsp        ; save user RSP
    mov rsp, [gs:0]        ; switch to kernel stack

    ; save registers
    push rcx               ; user RIP
    push r11               ; user RFLAGS
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; map syscall args to System V ABI
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax

    call c_syscall_handler

    ; exit half
    mov r12, rax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11
    pop rcx

    mov rdx, [gs:8]
    swapgs
    mov rsp, rdx
    mov rax, r12
    sysretq
