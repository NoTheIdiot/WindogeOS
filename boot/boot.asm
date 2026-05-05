MULTIBOOT_MAGIC         equ 0x1BADB002
MULTIBOOT_PAGE_ALIGN    equ 1 << 0
MULTIBOOT_RAM_MAP       equ 1 << 1
MULTIBOOT_HEADER_FLAGS  equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_RAM_MAP
MULTIBOOT_CHECK         equ -(MULTIBOOT_MAGIC + MULTIBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECK

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main

.halt:
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
