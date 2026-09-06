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

    ; Save the user return state and argument registers.
    ; SYSRET restores RIP from RCX and RFLAGS from R11.
    push r11
    push rcx
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    ; System call ABI: rax = syscall number, rdi = arg1, rsi = arg2, rdx = arg3.
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop rcx
    pop r11 

    call core_c_dispatcher

    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop rcx
    pop r11

    mov rsp, [gs:8]
    swapgs
    o64 sysret
