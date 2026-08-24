bits 64
global idt_load
global isr_stub_table
extern exception_handler

section .text

idt_load:
    lidt [rdi]
    ret

%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    push qword %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

isr_common:
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
    call exception_handler

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
isr_stub_table:
    dq isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3
    dq isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7
    dq isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11
    dq isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15
    dq isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19
    dq isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23
    dq isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27
    dq isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31