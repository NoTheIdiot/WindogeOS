bits 64

global to_userland_ring3
global to_userland_ring3_args
global syscall_exit_to_launcher
global kernel_program_launcher_rsp
global kernel_program_launcher_rflags
global syscall_entry

extern bsp_cpu_data
extern syscall_handler

MSR_IA32_KERNEL_GS_BASE equ 0xC0000102
MAX_LAUNCH_DEPTH        equ 8

section .bss
align 8
launcher_rsp_stack:
    resq MAX_LAUNCH_DEPTH
launcher_rflags_stack:
    resq MAX_LAUNCH_DEPTH
launcher_depth:
    resq 1
kernel_program_launcher_rsp:
    resq 1
kernel_program_launcher_rflags:
    resq 1

section .text
to_userland_ring3:
    ; Keep rdx (argc) and rcx (argv_user) intact!
to_userland_ring3_args:
    ; rdi = entry, rsi = stack_top, rdx = argc, rcx = argv_user
    mov r8, rdi
    mov r9, rsi
    mov r10, rdx
    mov r11, rcx

    cli

    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov rax, [rel launcher_depth]
    cmp rax, MAX_LAUNCH_DEPTH
    jae .launch_stack_full

    lea rdx, [rel launcher_rsp_stack]
    mov [rdx + rax*8], rsp
    mov [rel kernel_program_launcher_rsp], rsp

    pushfq
    pop rcx
    lea rdx, [rel launcher_rflags_stack]
    mov [rdx + rax*8], rcx
    mov [rel kernel_program_launcher_rflags], rcx

    inc rax
    mov [rel launcher_depth], rax

    mov ax, 0x1b
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x1b         ; user SS
    push r9                 ; user RSP
    push qword 0x202        ; user RFLAGS
    push qword 0x23         ; user CS
    push r8                 ; user RIP

    mov rdi, r10            ; argc -> RDI
    mov rsi, r11            ; argv_user -> RSI

    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    iretq

.launch_stack_full:
    hlt
    jmp .launch_stack_full

syscall_exit_to_launcher:
    cli

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    lea rax, [rel bsp_cpu_data]
    mov rdx, rax
    shr rdx, 32
    mov eax, eax
    mov ecx, MSR_IA32_KERNEL_GS_BASE
    wrmsr

    mov rax, [rel launcher_depth]
    test rax, rax
    jz .no_launcher_frame

    dec rax
    mov [rel launcher_depth], rax

    lea rdx, [rel launcher_rsp_stack]
    mov rsp, [rdx + rax*8]

    test rax, rax
    jz .clear_exported_rsp
    mov rcx, [rdx + rax*8 - 8]
    mov [rel kernel_program_launcher_rsp], rcx
    jmp .restore_rflags

.clear_exported_rsp:
    mov qword [rel kernel_program_launcher_rsp], 0

.restore_rflags:
    lea rdx, [rel launcher_rflags_stack]
    mov rcx, [rdx + rax*8]
    mov [rel kernel_program_launcher_rflags], rcx
    push rcx
    popfq

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    xor eax, eax
    ret

.no_launcher_frame:
    mov qword [rel kernel_program_launcher_rsp], 0
    hlt
    jmp .no_launcher_frame

syscall_entry:
    ; Save user RSP in [rel bsp_cpu_data + 8] via rax without touching user stack
    mov [rel bsp_cpu_data + 8], rsp
    mov rsp, [rel bsp_cpu_data + 0]

    swapgs

    push qword [rel bsp_cpu_data + 8]
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

    sti

    mov rdi, rsp
    call syscall_handler

    cli
    mov [rsp], rax

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

    pop r11
    pop rcx
    pop rsp

    swapgs

    o64 sysret