bits 64

global to_userland_ring3
global to_userland_ring3_args
global syscall_exit_to_launcher

extern bsp_cpu_data
extern kernel_program_launcher_rsp
extern kernel_program_launcher_rflags
MSR_IA32_KERNEL_GS_BASE equ 0xC0000102

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

    ; bookmark the launcher's current stack pointer so we don't jump to 0x0 and triple-fault on exit
    mov [rel kernel_program_launcher_rsp], rsp

    ; save the launcher's CPU flags (including whether interrupts were enabled before cli)
    pushfq
    pop rax
    mov [rel kernel_program_launcher_rflags], rax

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

syscall_exit_to_launcher:
    cli

    ; restore kernel data segment selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    ; point IA32_KERNEL_GS_BASE at bsp_cpu_data and swap GS back
    lea rax, [rel bsp_cpu_data]
    mov edx, eax
    shr rax, 32
    xchg eax, edx
    mov ecx, MSR_IA32_KERNEL_GS_BASE
    wrmsr
    swapgs

    ; restore launcher stack pointer and wipe saved pointer
    mov rsp, [rel kernel_program_launcher_rsp]
    mov qword [rel kernel_program_launcher_rsp], 0

    ; restore launcher RFLAGS
    mov rax, [rel kernel_program_launcher_rflags]
    push rax
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