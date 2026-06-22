ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    . = 0x100000;
    
    header_start = .;

    .text : ALIGN(4K) {
        text_start = .;
        *(.multiboot)
        *(.text)
    }

    .rodata : ALIGN(4K) {
        *(.rodata)
    }

    .data : ALIGN(4K) {
        *(.data)
    }

    .bss : ALIGN(4K) {
        *(.common)
        *(.bss)
        bss_end = .;
    }
}
