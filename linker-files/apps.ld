ENTRY(_start)

SECTIONS
{
    . = 0x00200000;

    .text : {
        *(.text._start)
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) }
}