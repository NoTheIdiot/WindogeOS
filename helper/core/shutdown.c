#include <basicutil.h>

void core_reboot(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("cli");

    __asm__ volatile(
        "mov $0x64, %%dx\n\t"
        "mov $0xFE, %%al\n\t"
        "outb %%al, %%dx\n\t"
        :
        :
        : "ax", "dx", "memory"
    );

    for (;;) {
        __asm__ volatile("hlt");
    }
#elif defined(__aarch64__) || defined(__riscv)
    halt();
#elif defined(__loongarch64)
    halt();
#else
    halt();
#endif
}

void core_shutdown(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("cli");

    __asm__ volatile(
        "mov $0x604, %%dx\n\t"
        "mov $0x2000, %%ax\n\t"
        "outw %%ax, %%dx\n\t"
        "mov $0xB004, %%dx\n\t"
        "outw %%ax, %%dx\n\t"
        :
        :
        : "ax", "dx", "memory"
    );

    for (;;) {
        __asm__ volatile("hlt");
    }
#elif defined(__aarch64__) || defined(__riscv)
    halt();
#elif defined(__loongarch64)
    halt();
#else
    halt();
#endif
}
