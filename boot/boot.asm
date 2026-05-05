MULTIBOOT_MAGIC         equ 0x1BADB002
MULTIBOOT_PAGE_ALIGN    equ 1 << 0
MULTIBOOT_MEMORY_INFO   equ 1 << 1
MULTIBOOT_VIDEO_MODE    equ 1 << 2
MULTIBOOT_HEADER_FLAGS  equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE
MULTIBOOT_CHECK         equ -(MULTIBOOT_MAGIC + MULTIBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECK
    dd 1          ; mode_type (1 = graphics)
    dd 1024       ; width
    dd 768        ; height
    dd 32         ; depth

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
