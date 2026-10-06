bits 64

global system_run_bin
global system_run_elf
global syscall_exit_to_launcher

extern kernel_program_launcher_rsp
extern kernel_program_launcher_rflags
extern system_run_bin_impl
extern system_run_elf_impl

section .text
system_run_bin:
    pushfq
    pop rax
    mov [rel kernel_program_launcher_rsp], rsp
    mov [rel kernel_program_launcher_rflags], rax
    sub rsp, 8
    call system_run_bin_impl
    add rsp, 8
    mov qword [rel kernel_program_launcher_rsp], 0
    mov qword [rel kernel_program_launcher_rflags], 0
    ret

system_run_elf:
    pushfq
    pop rax
    mov [rel kernel_program_launcher_rsp], rsp
    mov [rel kernel_program_launcher_rflags], rax
    sub rsp, 8
    call system_run_elf_impl
    add rsp, 8
    mov qword [rel kernel_program_launcher_rsp], 0
    mov qword [rel kernel_program_launcher_rflags], 0
    ret

syscall_exit_to_launcher:
    mov rax, rdi
    mov rdx, [rel kernel_program_launcher_rsp]
    mov rcx, [rel kernel_program_launcher_rflags]
    mov qword [rel kernel_program_launcher_rsp], 0
    mov qword [rel kernel_program_launcher_rflags], 0
    swapgs
    mov rsp, rdx
    push rcx
    popfq
    ret
