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

// port stuff
uint8_t ports_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void ports_outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint16_t ports_inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void ports_outw(uint16_t port, uint16_t data) {
    __asm__ volatile ("outw %0, %1" : : "a"(data), "Nd"(port));
}

void ports_outsw(unsigned short port, const void *addr, unsigned long count) {
    __asm__ volatile (
        "rep outsw"
        : "+S" (addr), "+c" (count)
        : "d" (port)
        : "memory"
    );
}

void ports_insw(unsigned short port, void *addr, unsigned long count) {
    __asm__ volatile (
        "rep insw"
        : "+D" (addr), "+c" (count)
        : "d" (port)
        : "memory"
    );
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
