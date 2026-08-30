; osdev when you cant find a single fucking repo to learn from that doesn't 
; have 30 different fucking headers and realizing that the project OS is directly
; inspired by linux so it's practically useless and you find the perfect repo with
; perfect code but relalize its gpl (syscall)

bits 64
global core_sys_entry
extern core_c_dispatcher

section .text
core_sys_entry:
    swapgs               ; Switch to kernel GS framework

    mov [gs:8], rsp     ; Back up the user space stack pointer
    mov rsp, [gs:0]      ; Load this core's safe kernel stack pointer

    ; 1. Preserve the CPU states and user arguments cleanly
    push rcx             ; User return RIP
    push r11             ; User RFLAGS
    push rdi             ; User Argument 1
    push rsi             ; User Argument 2
    push rdx             ; User Argument 3
    push r10             ; User Argument 4 (Passed in R10 for syscalls!)
    push r8              ; User Argument 5
    push r9              ; User Argument 6
    push rax             ; System Call ID Number

    ; 2. Map parameters to the C Calling Convention without corrupting them
    ; Target signature: core_c_dispatcher(syscall_num, arg1, arg2, arg3)
    mov rdi, rax         ; Parameter 1 = Syscall Number
    mov rsi, [rsp + 48]  ; Parameter 2 = Original RDI (read from stack safely)
    mov rdx, [rsp + 40]  ; Parameter 3 = Original RSI (read from stack safely)
    mov rcx, [rsp + 32]  ; Parameter 4 = Original RDX (read from stack safely)

    call core_c_dispatcher

    ; 3. Clean up the stack frame and restore states
    add rsp, 8           ; Skip RAX (keep the return value from C handler)
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop r11              ; Restore RFLAGS for SYSRET
    pop rcx              ; Restore return RIP for SYSRET

    mov rsp, [gs:8]      ; Switch back to the true user stack
    swapgs               ; Revert GS back to user mode
    sysretq
