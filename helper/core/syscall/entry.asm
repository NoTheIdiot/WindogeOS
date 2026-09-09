bits 64

global syscall_entry
extern syscall_handler

section .text
syscall_entry:
    swapgs

    ; get a user stack pointer and get the kernel stack pointer
    mov qword [gs:0], rsp
    mov rsp, qword [gs:8]

    ; save the r registers for x86_64
    push qword [gs:0]   
    push r15
    push r14
    push r13
    push r12
    push r11            
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx            
    push rbx
    push rax            

    mov rdi, rsp        
    
    call syscall_handler

    mov [rsp], rax        

    ; restore registers
    pop rax
    pop rbx
    pop rcx             
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11             
    pop r12
    pop r13
    pop r14
    pop r15

    pop rsp
    swapgs

    ; go back
    o64 sysret
