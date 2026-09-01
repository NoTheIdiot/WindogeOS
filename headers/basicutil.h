#ifndef BASICUTIL_H
#define BASICUTIL_H

// include headers
#include <stdint.h>
#include <stddef.h>

// really basic stuff
void halt();
void core_shutdown(void);
void core_reboot(void);

extern const char* doge_ascii[22];

// low level ports
static inline uint8_t ports_inb(uint16_t port) {
#if defined(__x86_64__) || defined(__i386__)
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
#elif defined(__aarch64__) || defined(_M_ARM64)
    (void)port;
    return 0; // ARM64 uses MMIO; port I/O is unsupported/stubbed
#else
    (void)port;
    return 0;
#endif
}

static inline void ports_outb(uint16_t port, uint8_t val) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
#elif defined(__aarch64__) || defined(_M_ARM64)
    (void)port;
    (void)val;
#else
    (void)port;
    (void)val;
#endif
}

static inline uint16_t ports_inw(uint16_t port) {
#if defined(__x86_64__) || defined(__i386__)
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
#elif defined(__aarch64__) || defined(_M_ARM64)
    (void)port;
    return 0;
#else
    (void)port;
    return 0;
#endif
}

static inline void ports_outw(uint16_t port, uint16_t data) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("outw %0, %1" : : "a"(data), "Nd"(port));
#elif defined(__aarch64__) || defined(_M_ARM64)
    (void)port;
    (void)data;
#else
    (void)port;
    (void)data;
#endif
}

static inline void ports_outsw(unsigned short port, const void *addr, unsigned long count) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile (
        "rep outsw"
        : "+S" (addr), "+c" (count)
        : "d" (port)
        : "memory"
    );
#elif defined(__aarch64__) || defined(_M_ARM64)
    (void)port;
    (void)addr;
    (void)count;
#else
    (void)port;
    (void)addr;
    (void)count;
#endif
}

static inline void ports_insw(unsigned short port, void *addr, unsigned long count) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile (
        "rep insw"
        : "+D" (addr), "+c" (count)
        : "d" (port)
        : "memory"
    );
#elif defined(__aarch64__) || defined(_M_ARM64)
    (void)port;
    (void)addr;
    (void)count;
#else
    (void)port;
    (void)addr;
    (void)count;
#endif
}

// serial stuff
void serial_init();
int serial_transmit_empty();
void serial_putchar(char c);
void serial_print(const char* str);
void serial_println(const char* str);

// basic
void log(const char* str);
void duolog(const char* str);
void panic(char* reason, char *file, int line);

// info
char* cpuid(void);

#endif