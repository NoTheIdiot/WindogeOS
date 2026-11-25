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
    ; zero out arg0 and arg1 when calling with no args
    xor edx, edx
    xor ecx, ecx

to_userland_ring3_args:
    ; hey, hoard these damn arguments
    mov r8, rdi
    mov r9, rsi
    mov r10, rdx
    mov r11, rcx

    ; NO ARGUMENTS
    cli

    ; save C callee-saved registers on kernel stack
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; push current launcher RSP and RFLAGS onto the nesting stack
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

    ; load Ring 3 user data segment selector (0x18 | RPL 3 = 0x1B) into data segment registers
    mov ax, 0x1b
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; build the fake interrupt stack
    push qword 0x1b         ; user SS (stack segment)
    push r9                 ; user RSP (top of user stack pointing to argc)
    push qword 0x202        ; user RFLAGS (0x200 = IF interrupts enabled, 0x002 = mandatory reserved bit)
    push qword 0x23         ; user CS (64-bit user code segment 0x20 | RPL 3 = 0x23)
    push r8                 ; user RIP (ELF entry point)

    ; Clear general purpose registers before entering userland (argc/argv are already on the stack via r9)
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rdi, rdi
    xor rsi, rsi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    ; pop RIP, CS, RFLAGS, RSP, SS and drop into Ring 3 userland
    iretq

.launch_stack_full:
    hlt
    jmp .launch_stack_full

syscall_exit_to_launcher:
    cli

    ; restore kernel data segment selectors (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; point IA32_KERNEL_GS_BASE at bsp_cpu_data
    lea rax, [rel bsp_cpu_data]
    mov rdx, rax
    shr rdx, 32
    mov eax, eax
    mov ecx, MSR_IA32_KERNEL_GS_BASE
    wrmsr

    ; pop one launcher frame from the nesting stack
    mov rax, [rel launcher_depth]
    test rax, rax
    jz .no_launcher_frame

    dec rax
    mov [rel launcher_depth], rax

    lea rdx, [rel launcher_rsp_stack]
    mov rsp, [rdx + rax*8]

    ; update exported kernel_program_launcher_rsp to the remaining outer frame (or 0)
    test rax, rax
    jz .clear_exported_rsp
    mov rcx, [rdx + rax*8 - 8]
    mov [rel kernel_program_launcher_rsp], rcx
    jmp .restore_rflags

.clear_exported_rsp:
    mov qword [rel kernel_program_launcher_rsp], 0

.restore_rflags:
    ; restore launcher RFLAGS and update exported copy
    lea rdx, [rel launcher_rflags_stack]
    mov rcx, [rdx + rax*8]
    mov [rel kernel_program_launcher_rflags], rcx
    push rcx
    popfq

    ; restore callee-saved registers in reverse order
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
    ; ensure IA32_KERNEL_GS_BASE points to bsp_cpu_data before swapping
    push rax
    push rcx
    push rdx
    lea rax, [rel bsp_cpu_data]
    mov rdx, rax
    shr rdx, 32
    mov eax, eax
    mov ecx, MSR_IA32_KERNEL_GS_BASE
    wrmsr
    pop rdx
    pop rcx
    pop rax

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