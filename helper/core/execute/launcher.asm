bits 64

%define MAX_SYSCALL_CONTEXT_DEPTH 64

global system_run_elf
global system_run_elf_args
global syscall_exit_to_launcher

extern syscall_context_depth
extern syscall_context_gs_bases
extern system_run_elf_impl
extern system_run_elf_args_impl
extern system_elf_post_cleanup

section .data
kernel_parent_rsps:    times 64 dq 0
kernel_parent_rflags:  times 64 dq 0

section .text

system_run_elf:
    mov ecx, [rel syscall_context_depth]
    cmp ecx, MAX_SYSCALL_CONTEXT_DEPTH
    jae .elf_overflow

    pushfq
    pop rax
    
    sub rsp, 8
    lea rdx, [rel kernel_parent_rsps]
    mov [rdx + rcx * 8], rsp
    
    lea rdx, [rel kernel_parent_rflags]
    mov [rdx + rcx * 8], rax

    inc dword [rel syscall_context_depth]
    call system_run_elf_impl
    
.elf_return_track:
    add rsp, 8
    call system_elf_post_cleanup
    xor rax, rax
    ret

.elf_overflow:
    mov rax, -1
    ret

system_run_elf_args:
    mov ecx, [rel syscall_context_depth]
    cmp ecx, MAX_SYSCALL_CONTEXT_DEPTH
    jae .args_overflow

    pushfq
    pop rax
    
    sub rsp, 8
    lea rdx, [rel kernel_parent_rsps]
    mov [rdx + rcx * 8], rsp
    
    lea rdx, [rel kernel_parent_rflags]
    mov [rdx + rcx * 8], rax

    inc dword [rel syscall_context_depth]
    call system_run_elf_args_impl
    
.args_return_track:
    add rsp, 8
    call system_elf_post_cleanup
    xor rax, rax
    ret

.args_overflow:
    mov rax, -1
    ret

syscall_exit_to_launcher:
    mov ecx, [rel syscall_context_depth]
    dec ecx
    mov [rel syscall_context_depth], ecx

    lea rdx, [rel kernel_parent_rsps]
    mov r8, [rdx + rcx * 8]
    mov qword [rdx + rcx * 8], 0

    lea rdx, [rel kernel_parent_rflags]
    mov r9, [rdx + rcx * 8]
    mov qword [rdx + rcx * 8], 0

    mov rdi, rcx 

    mov rsp, r8
    push r9
    popfq
    ret
