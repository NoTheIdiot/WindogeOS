[bits 64]
section .text

global idt_load
idt_load:
    lidt [rdi]
    ret

extern isr_common_handler

%macro ISR_NOERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push qword 0          
    push qword %1         
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push qword %1         
    jmp isr_common_stub
%endmacro


ISR_NOERRCODE 0   
ISR_NOERRCODE 1   
ISR_NOERRCODE 2   
ISR_NOERRCODE 3   
ISR_NOERRCODE 4   
ISR_NOERRCODE 5   
ISR_NOERRCODE 6   
ISR_NOERRCODE 7   
ISR_ERRCODE   8   
ISR_NOERRCODE 9   
ISR_ERRCODE   10  
ISR_ERRCODE   11  
ISR_ERRCODE   12  
ISR_ERRCODE   13  
ISR_ERRCODE   14  
ISR_NOERRCODE 15  
ISR_NOERRCODE 16  
ISR_ERRCODE   17  
ISR_NOERRCODE 18  
ISR_NOERRCODE 19  
ISR_NOERRCODE 20  
ISR_ERRCODE   21  
%assign i 22
%rep 10
ISR_NOERRCODE i
%assign i i+1
%endrep

isr_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp           
    call isr_common_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16            
    iretq

section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 32
    dq isr_stub_%[i]
%assign i i+1
%endrep