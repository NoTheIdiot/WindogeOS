; bits 64

; global system_run_elf
; global syscall_exit_to_launcher

; extern run_elf

; section .bss
; saved_rsp: resq 1

; section .text

; system_run_elf:
    ; mov [rel saved_rsp], rsp
;    call run_elf
;     ret
; 
; syscall_exit_to_launcher:
    ; mov rsp/, [rel saved_rsp]
    ; xor eax, eax
    ; ret