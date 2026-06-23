OUTPUT_FORMAT("binary")
ENTRY(main)

SECTIONS
{
    . = 0x00200000; 

    .text : {
        *(.text)
    }
    .data : {
        *(.data)
    }
    .bss : {
        *(.bss)
    }
}
