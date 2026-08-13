#include <basicutil.h>
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
