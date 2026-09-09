#include <system.h>
#include <dogeio.h>
#include <string.h>

#define IA32_EFER  0xC0000080
#define IA32_STAR  0xC0000081
#define IA32_LSTAR 0xC0000082
#define IA32_FMASK 0xC0000084

extern void syscall_entry();

struct cpu_regs {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15, user_rsp;
};

static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
    return ((uint64_t)high << 32) | low;
}

static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile("wrmsr" : : "c" (msr), "a" (low), "d" (high));
}

uint64_t syscall_handler(struct cpu_regs *regs) {
    uint64_t sc_num = regs->rax;
    uint64_t ret_val = 0;

    switch (sc_num) {
        case FS_READ: {
            char* filepath = (char*)regs->rdi;
            char* buffer   = (char*)regs->rsi;
            uint32_t size  = (uint32_t)regs->rdx;

            ret_val = fs_read(filepath, buffer, size);
            break;
        }

        case FS_WRITE: {
            char* filepath = (char*)regs->rdi;
            char* buffer   = (char*)regs->rsi;

            ret_val = fs_write(filepath, buffer);
            break;
        }
        
        default:
            ret_val = (uint64_t)-1; 
            break;
    }

    return ret_val;
}

void syscall_init(void) {
    uint64_t efer = read_msr(IA32_EFER);
    write_msr(IA32_EFER, efer | 1);

    uint64_t star = 0;
    star |= ((uint64_t)0x08 << 32); 
    star |= ((uint64_t)0x13 << 48); 
    write_msr(IA32_STAR, star);

    write_msr(IA32_LSTAR, (uint64_t)syscall_entry);
    write_msr(IA32_FMASK, 0x200);
}