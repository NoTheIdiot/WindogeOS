AALIGN		equ 1 << 0
MEMINFO		equ 1 << 1
FLAGS		equ AALIGN | MEMINFO
MAGIC 		equ 0x1BADB002
CHECK		equ -(MAGIC + FLAGS)

section .multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECK

section .text
extern kernel_main
global _start
_start:
	mov esp, stack_top
	call kernel_main
.hang
	hlt
	jmp .hang

section .bss
align 16
stack_bottom:
	resb 16384
stack_top:
