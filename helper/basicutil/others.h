#ifndef HELPER_BASICUTIL_OTHERS_H
#define HELPER_BASICUTIL_OTHERS_H

#include <stdint.h>

static inline void cli(void) {
    __asm__ volatile ("cli");
}

static inline void sti(void) {
    __asm__ volatile ("sti");
}

static inline void nop(void) {
    __asm__ volatile ("nop");
}

static inline void ltr(uint16_t selector) {
    __asm__ __volatile__("ltr %0" : : "r"(selector));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

#endif