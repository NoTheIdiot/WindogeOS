bits 64

global syscall_entry
extern syscall_handler

section .text
syscall_entry:
    swapgs

    ; Save user RSP in GS scratch and switch to kernel stack
    mov qword [gs:8], rsp
    mov rsp, qword [gs:0]

    ; preserve user RSP on the kernel stack so [gs:8] can't be  C R A C K E D  later
    push qword [gs:8]

    push rcx                ; user RIP (saved by SYSCALL)
    push r11                ; user RFLAGS (saved by SYSCALL)

    push r15
    push r14
    push r13
    push r12
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rbx
    push rax                ; syscall number

    sti                     ; safe once kernel stack & frame are set up

    mov rdi, rsp            ; pointer to saved register frame
    call syscall_handler

    cli
    mov [rsp], rax          ; store syscall return value in saved RAX slot

    pop rax
    pop rbx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r12
    pop r13
    pop r14
    pop r15

    pop r11                 ; restore user RFLAGS
    pop rcx                 ; restore user RIP

    pop rsp                 ; restore user RSP from kernel stack
    swapgs

    o64 sysret