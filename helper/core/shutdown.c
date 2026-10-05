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
#elif defined(__aarch64__)
    register uint64_t x0 __asm__("x0") = 0x84000009; 
    register uint64_t x1 __asm__("x1") = 0;
    register uint64_t x2 __asm__("x2") = 0;
    register uint64_t x3 __asm__("x3") = 0;
    
    __asm__ volatile("smc #0\n\thvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
    
    volatile uint32_t* rst_reg = (volatile uint32_t*)0x09000000;
    *rst_reg = 0x34; 

    for (;;) { __asm__ volatile("wfe"); }
#elif defined(__riscv)
    register uint64_t a0 __asm__("a0") = 0;          
    register uint64_t a1 __asm__("a1") = 0;          
    register uint64_t a6 __asm__("a6") = 0;          
    register uint64_t a7 __asm__("a7") = 0x53525354; 
    
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a6), "r"(a7) : "memory");
    
    volatile uint32_t* riscv_rst = (volatile uint32_t*)0x100000;
    *riscv_rst = 0x7777; 

    for (;;) { __asm__ volatile("wfi"); }
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
#elif defined(__aarch64__)
    register uint64_t x0 __asm__("x0") = 0x84000008; 
    register uint64_t x1 __asm__("x1") = 0;
    register uint64_t x2 __asm__("x2") = 0;
    register uint64_t x3 __asm__("x3") = 0;
    
    __asm__ volatile("smc #0\n\thvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
    
    volatile uint32_t* pwr_reg = (volatile uint32_t*)0x09000000;
    *pwr_reg = 0x30; 

    for (;;) { __asm__ volatile("wfe"); }
#elif defined(__riscv)
    register uint64_t a0 __asm__("a0") = 0;          
    register uint64_t a1 __asm__("a1") = 0;          
    register uint64_t a6 __asm__("a6") = 0;          
    register uint64_t a7 __asm__("a7") = 0x53525354; 
    
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a6), "r"(a7) : "memory");

    volatile uint32_t* riscv_pwr = (volatile uint32_t*)0x100000;
    *riscv_pwr = 0x5555; 

    for (;;) { __asm__ volatile("wfi"); }
#elif defined(__loongarch64)
    halt();
#else
    halt();
#endif
}
