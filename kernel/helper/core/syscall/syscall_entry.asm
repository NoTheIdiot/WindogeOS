bits 64

global syscall_entry
extern syscall_handler

section .text
syscall_entry:
    swapgs                                    

    mov qword [gs:8], rsp                 
    mov rsp, qword [gs:0]                 

    push qword 0x1b                       ; ss
    push qword [gs:8]                     ; rsp
    push r11                              ; rflags (saved by CPU in r11)
    push qword 0x23                       ; cs
    push rcx                              ; rip (saved by CPU in rcx)
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
    
    mov [rsp + 112], rax                  
    
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
    
    pop rcx                               ; restore user RIP into rcx for sysret
    add rsp, 8                            ; skip saved cs slot
    pop r11                               ; restore user RFLAGS into r11 for sysret
    pop rsp                               ; restore user RSP directly
    
    swapgs                                

    o64 sysret