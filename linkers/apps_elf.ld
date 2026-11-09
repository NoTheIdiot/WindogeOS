OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(0x1000) {
        *(.text._start)
        *(.text*)
        *(.ltext._start)
        *(.ltext*)
    }

    . = ALIGN(0x1000);
    .rodata : ALIGN(0x1000) {
        *(.rodata*)
        *(.lrodata*)
    }

    . = ALIGN(0x1000);
    .data : ALIGN(0x1000) {
        *(.data*)
        *(.ldata*)
    }

    . = ALIGN(0x1000);
    .bss : ALIGN(0x1000) {
        *(COMMON)
        *(.bss*)
        *(.lbss*)
    }

    _kernel_end = ALIGN(0x1000);

    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
        *(.comment)
    }
}
