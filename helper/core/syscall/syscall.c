#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <core.h>

#define MSR_IA32_EFER   0xC0000080
#define MSR_IA32_STAR   0xC0000081
#define MSR_IA32_LSTAR  0xC0000082
#define MSR_IA32_FMASK  0xC0000084

#define EFER_SCE        (1ULL << 0)
#define RFLAGS_IF       (1ULL << 9)

extern void syscall_entry(void);

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

struct cpu_regs {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15, user_rsp;
};

uint64_t syscall_handler(struct cpu_regs *regs) {
    uint64_t sc_num = regs->rax;
    uint64_t ret_val = 0;

    switch (sc_num) {
        case FS_READ: {
            char* filepath = (char*)regs->rdi;
            char* buffer   = (char*)regs->rsi;
            uint32_t size  = (uint32_t)regs->rdx;

            ret_val = (uint64_t)(int64_t)fs_read(filepath, buffer, size);
            break;
        }

        case FS_WRITE: {
            char* filepath = (char*)regs->rdi;
            char* buffer   = (char*)regs->rsi;

            ret_val = (uint64_t)(int64_t)fs_write(filepath, buffer);
            break;
        }
        
        default:
            ret_val = (uint64_t)-1; 
            break;
    }

    return ret_val;
}

void init_syscalls(void) {
    wrmsr(MSR_IA32_EFER, rdmsr(MSR_IA32_EFER) | EFER_SCE);

    uint64_t kernel_cs = 0x08; 
    uint64_t user_base = 0x1B; 
    
    uint64_t star = (kernel_cs << 32) | (user_base << 48);
    wrmsr(MSR_IA32_STAR, star);

    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_IA32_FMASK, RFLAGS_IF);
}