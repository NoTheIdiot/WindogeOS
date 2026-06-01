ld -m elf_i386 -T linker/linker.ld -o windoge.bin \
    boot.o \
    kernel.o idt.o data.o info.o serial.o dogescript.o \
    dogeshell.o dogeio.o ports.o string.o suchboot.o time.o \
    vbe.o dogeio_graphics.o font.o file.o