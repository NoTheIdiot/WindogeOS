[bits 64]

global syscall_entry
extern c_syscall_handler
extern user_rsp_scratch
extern kernel_stack_top

section .text

syscall_entry:
    ; save user stack pointer and switch to kernel stack
    mov qword [rel user_rsp_scratch], rsp
    mov rsp, qword [rel kernel_stack_top]

    ; save regeisters clobbered by SYSCALL or needed for SYSRET
    ; syscall is to go from ring 3 to ring 0
    ; sysret is to GO to ring 3

    push rcx          ; user RIP
    push r11          ; user RFLAGS
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; map system V AMD64 ABI args (holy alphabet soup) for c_syscall_handler:
    mov r8, r10       ; arg4 (r10) -> 5th argument (r8)
    mov rcx, rdx      ; arg3 (rdx) -> 4th argument (rcx)
    mov rdx, rsi      ; arg2 (rsi) -> 3rd argument (rdx)
    mov rsi, rdi      ; arg1 (rdi) -> 2nd argument (rsi)
    mov rdi, rax      ; sys_id (rax) -> 1st argument (rdi)

    ; into C 
    call c_syscall_handler 

    ; save return value in r12 temporarily in r12
    mov r12, rax

    ; SWITCH TO USER STACK IDIOT
    mov rsp, qword [rel user_rsp_scratch]

    ; put the return value back to rax
    mov rax, r12

    ; restore the dang regs
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11           ; restores user RFLAGS
    pop rcx           ; restores user RIP

    ; return to ring 3 (userspace), safely :D
    sysretq