; osdev when you cant find a single fucking repo to learn from that doesn't 
; have 30 different fucking headers and realizing that the project OS is directly
; inspired by linux so it's practically useless and you find the perfect repo with
; perfect code but relalize its gpl (syscall)

bits 64
global core_sys_entry
extern core_c_dispatcher

section .text
core_sys_entry:
    swapgs               

    mov [gs:8], rsp     
    mov rsp, [gs:0]      

    push rcx             
    push r11             
    push rdi             
    push rsi             
    push rdx             
    push r10             
    push r8              
    push r9              
    push rax             

    mov rdi, rax         
    mov rsi, [rsp + 48]  
    mov rdx, [rsp + 40]  
    mov rcx, [rsp + 32]  

    call core_c_dispatcher

    add rsp, 8           
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop r11              
    pop rcx              

    mov rsp, [gs:8]      
    swapgs               
    o64 sysret
