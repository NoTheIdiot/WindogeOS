OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS((1 << 0) | (1 << 2)); /* Read + Execute */
    data PT_LOAD FLAGS((1 << 1) | (1 << 0)); /* Read + Write */
}

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(0x1000) {
        *(.text._start)
        *(.text*)
        *(.ltext._start)
        *(.ltext*)
    } :text

    .rodata : ALIGN(0x1000) {
        *(.rodata*)
        *(.lrodata*)
    } :text

    .data : ALIGN(0x1000) {
        *(.data*)
        *(.ldata*)
    } :data

    .bss : ALIGN(0x1000) {
        *(COMMON)
        *(.bss*)
        *(.lbss*)
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
        *(.comment)
    }
}