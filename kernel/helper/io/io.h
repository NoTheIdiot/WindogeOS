#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <stddef.h>

static inline uint8_t inb(uint16_t port) {
#if defined(__x86_64__) || defined(__i386__)
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
#elif defined(__aarch64__) || defined(_M_ARM64)
    (void)port;
    return 0;
#else
    (void)port;
    return 0;
#endif
}

static inline void outb(uint16_t port, uint8_t val) {
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

static inline uint16_t inw(uint16_t port) {
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

static inline void outw(uint16_t port, uint16_t data) {
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

static inline void outsw(unsigned short port, const void *addr, unsigned long count) {
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

static inline void insw(unsigned short port, void *addr, unsigned long count) {
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

static inline void outl(uintptr_t addr, uint32_t val) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"((uint16_t)addr));
#elif defined(__aarch64__) || defined(__arm__)
#if defined(__aarch64__)
    __asm__ volatile ("dsb sy" ::: "memory");
#else
    __asm__ volatile ("dsb" ::: "memory");
#endif
    *(volatile uint32_t *)addr = val;
#if defined(__aarch64__)
    __asm__ volatile ("dsb sy" ::: "memory");
#else
    __asm__ volatile ("dsb" ::: "memory");
#endif
#endif
}

static inline uint32_t inl(uintptr_t addr) {
#if defined(__x86_64__) || defined (__i386__)
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"((uint16_t)addr));
    return ret;
#elif defined(__aarch64__) || defined(__arm__)
    uint32_t val = *(volatile uint32_t *)addr;
#if defined(__aarch64__)
    __asm__ volatile ("dmb sy" ::: "memory");
#else
    __asm__ volatile ("dmb" ::: "memory");
#endif
    return val;
#endif
}

static inline void io_barrier(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("mfence" ::: "memory");
#elif defined(__aarch64__) || defined(_M_ARM64)
    __asm__ volatile ("dsb sy" ::: "memory");
#else
    __asm__ volatile ("" ::: "memory");
#endif
}

static inline uint8_t mmio_read8(uintptr_t addr) {
    uint8_t ret = *(volatile uint8_t *)addr;
    io_barrier();
    return ret;
}

static inline void mmio_write8(uintptr_t addr, uint8_t val) {
    *(volatile uint8_t *)addr = val;
    io_barrier();
}

static inline uint16_t mmio_read16(uintptr_t addr) {
    uint16_t ret = *(volatile uint16_t *)addr;
    io_barrier();
    return ret;
}

static inline void mmio_write16(uintptr_t addr, uint16_t val) {
    *(volatile uint16_t *)addr = val;
    io_barrier();
}

static inline uint32_t mmio_read32(uintptr_t addr) {
    uint32_t ret = *(volatile uint32_t *)addr;
    io_barrier();
    return ret;
}

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
    io_barrier();
}

static inline uint64_t mmio_read64(uintptr_t addr) {
    uint64_t ret = *(volatile uint64_t *)addr;
    io_barrier();
    return ret;
}

static inline void mmio_write64(uintptr_t addr, uint64_t val) {
    *(volatile uint64_t *)addr = val;
    io_barrier();
}

static inline void mmio_read_buffer32(uintptr_t addr, uint32_t *buffer, size_t count) {
    volatile uint32_t *port = (volatile uint32_t *)addr;
    for (size_t i = 0; i < count; i++) {
        buffer[i] = *port;
    }
    io_barrier();
}

static inline void mmio_write_buffer32(uintptr_t addr, const uint32_t *buffer, size_t count) {
    volatile uint32_t *port = (volatile uint32_t *)addr;
    for (size_t i = 0; i < count; i++) {
        *port = buffer[i];
    }
    io_barrier();
}

#endif
