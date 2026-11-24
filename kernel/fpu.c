#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>

void init_fpu(void) {
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);
    cr0 |=  (1ULL << 1);
    cr0 |=  (1ULL << 5);
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0) : "memory");

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);
    cr4 |= (1ULL << 10);
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4) : "memory");

    __asm__ volatile ("fninit" ::: "memory");
}