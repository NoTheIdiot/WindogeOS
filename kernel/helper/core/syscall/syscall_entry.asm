bits 64

global syscall_entry
extern syscall_handler

section .text
syscall_entry:
    swapgs                                  

    mov qword [gs:8], rsp                 
    mov rsp, qword [gs:0]                 

    push rcx
    push r11

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
    push rax 

    mov rdi, rsp 
    
    call syscall_handler
    
    mov [rsp], rax 
    
    ; pop in reverse
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
    
    ; restore r flags
    pop r11                               
    pop rcx                               
    
    mov rsp, qword [gs:8]                 
    swapgs                                  

    o64 sysret