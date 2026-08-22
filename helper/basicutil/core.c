#include <basicutil.h>
#include <string.h>
#include <dogeio.h>
#include <stdint.h>

void halt(void) {
    for (;;) {
#if defined (__x86_64__) || defined (__i386__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");

// i wonder why im including loongarch64
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

// debugging logging
// also this is copied from the old code cuz it works
// use qemu for this dumbass
void log(const char* str) {
    serial_print(str);
    serial_print("\n");
}

// same thing but it prints both in the OS and in serial
void duolog(const char* str) {
    dogeio_text_println(str);
    serial_println(str);
}

// kernel panic (basic)
void panic(char* reason, char *file, int line) {
    dogeio_text_clear_raw();
    dogeio_text_println("====== KERNEL PANIC, YOU MAY CRY. ======");
    dogeio_text_print("Exception: ");
    dogeio_text_println(reason);
    dogeio_text_print("File: ");
    dogeio_text_println(file);
    dogeio_text_print("Line: ");
    char* linestr;
    str_itoa(line, linestr);
    dogeio_text_println(linestr);
}