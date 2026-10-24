#include <basicutil.h>
#include <string.h>
#include <dogeio.h>
#include <stdint.h>

void halt(void) {
    for (;;) {
#if defined (__x86_64__) || defined (__i386__)
        asm volatile ("cli; hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm volatile ("wfi");
// i wonder why im including loongarch64
#elif defined (__loongarch64)
        asm volatile ("idle 0");
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
    dogeio_println(str);
    serial_print(str);
    serial_print("\n");
}

// kernel panic (basic)
void panic(char* reason, char *file, int line) {
    dogeio_clear_raw();
    dogeio_println("====== KERNEL PANIC, YOU MAY CRY. ======");
    
    dogeio_print("Exception: ");
    dogeio_println(reason);
    
    dogeio_print("File: ");
    dogeio_println(file);
    
    dogeio_print("Line: ");

    char linestr[32];
    str_itoa(line, linestr);
    dogeio_println(linestr);

    log("oh god: kernel exception triggered");
    log(reason);


    halt();
}