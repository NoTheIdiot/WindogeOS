bits 64

global system_run_elf

extern run_elf

section .bss
saved_rsp: resq 1

section .text

system_run_elf:
    mov [rel saved_rsp], rsp
    call run_elf
    ret