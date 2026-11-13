#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <bool.h>
#include <syscalls.h>
#include <limine.h>

#define MSR_IA32_EFER            0xC0000080
#define MSR_IA32_STAR            0xC0000081
#define MSR_IA32_LSTAR           0xC0000082
#define MSR_IA32_FMASK           0xC0000084
#define MSR_IA32_KERNEL_GS_BASE  0xC0000102

#define EFER_SCE                 (1ULL << 0)
#define EFER_NXE                 (1ULL << 11)

#define RFLAGS_TF                (1ULL << 8)
#define RFLAGS_IF                (1ULL << 9)
#define RFLAGS_DF                (1ULL << 10)

#define SYS_EXIT                 60

#define USER_CODE_BASE  0x0000000000400000ULL
#define USER_STACK_BASE 0x00007FFFF0000000ULL
#define USER_ADDRESS_LIMIT 0x0000800000000000ULL
#define USER_IO_LIMIT 65536
#define USER_STRING_LIMIT 4096
#define USER_PATH_LIMIT 256
#define USER_INPUT_LIMIT 256

extern void syscall_entry(void);
extern volatile struct limine_hhdm_request hhdm_request;

typedef struct {
    uint64_t kernel_rsp;
    uint64_t user_rsp_scratch;
} __attribute__((packed)) per_cpu_data_t;

static uint8_t syscall_stack[65536] __attribute__((aligned(16)));
static per_cpu_data_t bsp_cpu_data;
uint64_t kernel_program_launcher_rsp = 0;
uint64_t kernel_program_launcher_rflags = 0;
static bool cpu_supports_nx;
static bool cpu_supports_rdrand;

__attribute__((noreturn)) extern void syscall_exit_to_launcher(void);

uint64_t syscall_handler(registers_t *regs) {

    uint64_t sc_num = regs->rax;
    uint64_t ret_val = 0;

    switch (sc_num) {
        case Process_Exit: {
            cleanup_pages();
            syscall_exit_to_launcher();
            ret_val = 0;
            break;
        }

        case Print: {
            print("Programs work\n");
            ret_val = 0;
            break;
        }

        default:
            print("[\033[91mNot Wow\033[0m] Unknown system call tried to execute, but doesn't have id. ");
            sprint("Unknown system call tried to execute, but doesn't have id. ");
            char buffer[4];
            itoa((int)sc_num, buffer);
            println(buffer);
            sprintln(buffer);
            break;
    }

    return ret_val;
}

void init_syscalls(void) {
    uint32_t max_basic_leaf;
    uint32_t cpuid_ebx;
    uint32_t cpuid_ecx;
    uint32_t cpuid_edx;
    __asm__ volatile (
        "cpuid"
        : "=a"(max_basic_leaf), "=b"(cpuid_ebx),
          "=c"(cpuid_ecx), "=d"(cpuid_edx)
        : "a"(0), "c"(0)
    );
    (void)cpuid_ebx;
    (void)cpuid_edx;
    if (max_basic_leaf >= 1) {
        uint32_t basic_features;
        __asm__ volatile (
            "cpuid"
            : "=a"(basic_features), "=b"(cpuid_ebx),
              "=c"(cpuid_ecx), "=d"(cpuid_edx)
            : "a"(1), "c"(0)
        );
        cpu_supports_rdrand = (cpuid_ecx & (1U << 30)) != 0;
    }

    uint32_t max_extended_leaf;
    __asm__ volatile (
        "cpuid"
        : "=a"(max_extended_leaf), "=b"(cpuid_ebx),
          "=c"(cpuid_ecx), "=d"(cpuid_edx)
        : "a"(0x80000000U), "c"(0)
    );
    (void)cpuid_ebx;
    (void)cpuid_ecx;
    if (max_extended_leaf >= 0x80000001U) {
        uint32_t extended_features;
        __asm__ volatile (
            "cpuid"
            : "=a"(extended_features), "=b"(cpuid_ebx),
              "=c"(cpuid_ecx), "=d"(cpuid_edx)
            : "a"(0x80000001U), "c"(0)
        );
        (void)extended_features;
        (void)cpuid_ebx;
        (void)cpuid_ecx;
        cpu_supports_nx = (cpuid_edx & (1U << 20)) != 0;
    }
    if (cpu_supports_nx) {
        wrmsr(MSR_IA32_EFER, rdmsr(MSR_IA32_EFER) | EFER_NXE);
    }

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