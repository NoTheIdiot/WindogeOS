#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <bool.h>
#include <core.h>
#include <basicutil.h>

#define MSR_IA32_EFER            0xC0000080
#define MSR_IA32_STAR            0xC0000081
#define MSR_IA32_LSTAR           0xC0000082
#define MSR_IA32_FMASK           0xC0000084
#define MSR_IA32_KERNEL_GS_BASE  0xC0000102

#define EFER_SCE                 (1ULL << 0)

#define RFLAGS_TF                (1ULL << 8)
#define RFLAGS_IF                (1ULL << 9)
#define RFLAGS_DF                (1ULL << 10)

#define SYS_EXIT                 60

#define USER_CODE_BASE  0x0000000000400000ULL
#define USER_STACK_BASE 0x00007FFFF0000000ULL
#define PAGE_SIZE       4096

extern void syscall_entry(void);

typedef struct {
    uint64_t kernel_rsp;
    uint64_t user_rsp_scratch;
} __attribute__((packed)) per_cpu_data_t;

static uint8_t syscall_stack[16384] __attribute__((aligned(16)));
static per_cpu_data_t bsp_cpu_data;
uint64_t kernel_program_launcher_rsp = 0;

struct cpu_regs {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15, user_rsp;
};

static inline bool is_user_address(const void *ptr) {
    return ptr != NULL && (uint64_t)ptr < 0x0000800000000000ULL;
}

uint64_t syscall_handler(struct cpu_regs *regs) {
    uint64_t sc_num = regs->rax;
    uint64_t ret_val = 0;

    switch (sc_num) {
        case SYS_EXIT: {
            uint64_t exit_code = regs->rdi;
            __asm__ volatile (
                "mov %0, %%rax\n\t"
                "mov %1, %%rsp\n\t"
                "ret"
                :
                : "r"(exit_code), "m"(kernel_program_launcher_rsp)
                : "rax"
            );
            while(1);
        }

        case FS_READ: {
            char* filepath = (char*)regs->rdi;
            char* buffer   = (char*)regs->rsi;
            uint32_t size  = (uint32_t)regs->rdx;

            if (!is_user_address(filepath) || !is_user_address(buffer)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_read(filepath, buffer, size);
            break;
        }

        case FS_WRITE: {
            char* filepath = (char*)regs->rdi;
            char* buffer   = (char*)regs->rsi;

            if (!is_user_address(filepath) || !is_user_address(buffer)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_write(filepath, buffer);
            break;
        }

        case SPECIAL: {
            dogeio_text_print("special");
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
    uint64_t user_base = 0x10;
    
    uint64_t star = (kernel_cs << 32) | (user_base << 48);
    wrmsr(MSR_IA32_STAR, star);

    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_IA32_FMASK, RFLAGS_IF | RFLAGS_TF | RFLAGS_DF);

    bsp_cpu_data.kernel_rsp = (uint64_t)&syscall_stack[sizeof(syscall_stack)];
    bsp_cpu_data.user_rsp_scratch = 0;

    wrmsr(MSR_IA32_KERNEL_GS_BASE, (uint64_t)&bsp_cpu_data);
}
