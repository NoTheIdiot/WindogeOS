bits 64

%define MAX_SYSCALL_CONTEXT_DEPTH 4

global system_run_bin
global system_run_bin_args
global system_run_elf
global system_run_elf_args
global syscall_exit_to_launcher

extern kernel_program_launcher_rsp
extern kernel_program_launcher_rflags
extern syscall_context_depth
extern syscall_context_gs_bases
extern system_run_bin_impl
extern system_run_bin_args_impl
extern system_run_elf_impl
extern system_run_elf_args_impl

section .text
system_run_bin:
    cmp dword [rel syscall_context_depth], MAX_SYSCALL_CONTEXT_DEPTH
    jae .context_overflow
    inc dword [rel syscall_context_depth]
    pushfq
    pop rax
    mov [rel kernel_program_launcher_rsp], rsp
    mov [rel kernel_program_launcher_rflags], rax
    sub rsp, 8
    call system_run_bin_impl
    add rsp, 8
    mov qword [rel kernel_program_launcher_rsp], 0
    mov qword [rel kernel_program_launcher_rflags], 0
    dec dword [rel syscall_context_depth]
    ret
.context_overflow:
    mov rax, -1
    ret

system_run_bin_args:
    cmp dword [rel syscall_context_depth], MAX_SYSCALL_CONTEXT_DEPTH
    jae .context_overflow
    inc dword [rel syscall_context_depth]
    pushfq
    pop rax
    mov [rel kernel_program_launcher_rsp], rsp
    mov [rel kernel_program_launcher_rflags], rax
    sub rsp, 8
    call system_run_bin_args_impl
    add rsp, 8
    mov qword [rel kernel_program_launcher_rsp], 0
    mov qword [rel kernel_program_launcher_rflags], 0
    dec dword [rel syscall_context_depth]
    ret
.context_overflow:
    mov rax, -1
    ret

system_run_elf:
    cmp dword [rel syscall_context_depth], MAX_SYSCALL_CONTEXT_DEPTH
    jae .context_overflow
    inc dword [rel syscall_context_depth]
    pushfq
    pop rax
    mov [rel kernel_program_launcher_rsp], rsp
    mov [rel kernel_program_launcher_rflags], rax
    sub rsp, 8
    call system_run_elf_impl
    add rsp, 8
    mov qword [rel kernel_program_launcher_rsp], 0
    mov qword [rel kernel_program_launcher_rflags], 0
    dec dword [rel syscall_context_depth]
    ret
.context_overflow:
    mov rax, -1
    ret

system_run_elf_args:
    cmp dword [rel syscall_context_depth], MAX_SYSCALL_CONTEXT_DEPTH
    jae .context_overflow
    inc dword [rel syscall_context_depth]
    pushfq
    pop rax
    mov [rel kernel_program_launcher_rsp], rsp
    mov [rel kernel_program_launcher_rflags], rax
    sub rsp, 8
    call system_run_elf_args_impl
    add rsp, 8
    mov qword [rel kernel_program_launcher_rsp], 0
    mov qword [rel kernel_program_launcher_rflags], 0
    dec dword [rel syscall_context_depth]
    ret
.context_overflow:
    mov rax, -1
    ret

syscall_exit_to_launcher:
    mov r11, rdi
    mov r8, [rel kernel_program_launcher_rsp]
    mov r9, [rel kernel_program_launcher_rflags]
    mov qword [rel kernel_program_launcher_rsp], 0
    mov qword [rel kernel_program_launcher_rflags], 0

    mov ecx, [rel syscall_context_depth]
    dec ecx
    lea rdx, [rel syscall_context_gs_bases]
    mov r10, [rdx + rcx * 8]

    mov ecx, 0xC0000101
    mov eax, r10d
    mov rdx, r10
    shr rdx, 32
    wrmsr

    mov ecx, 0xC0000102
    xor eax, eax
    xor edx, edx
    wrmsr

    mov rax, r11
    mov rsp, r8
    push r9
    popfq
    ret
