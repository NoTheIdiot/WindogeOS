#include <stdint.h>
#include <dogeio.h>
#include <boot/syscall.h>

#define MSR_EFER         0xC0000080
#define MSR_STAR         0xC0000081
#define MSR_LSTAR        0xC0000082
#define MSR_SFMASK       0xC0000084
#define MSR_KERNEL_GS    0xC0000102

extern void core_sys_entry(void);
extern void core_to_user(uint64_t cr3_phys, void *entry_point, void *user_stack);

static volatile uint8_t temporary_syscall_stack[16384] __attribute__((aligned(16)));
uint64_t shell_rsp_backup = 0;
uint64_t shell_rbp_backup = 0;
uint64_t shell_rip_backup = 0;
uint64_t shell_cr3_backup = 0;

typedef struct {
    uint64_t kernel_stack_top;     
    uint64_t user_stack_scratch;   
} __attribute__((packed)) core_cpu_data_t;

static core_cpu_data_t bsp_cpu_storage;

uint64_t core_c_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg2;
    (void)arg3;
    switch (syscall_num) {
        case DOGEIO_TEXT_EXIT:
            __asm__ volatile("mov %0, %%cr3" :: "r"(shell_cr3_backup));
            __asm__ volatile (
                "mov %0, %%rsp\n\t"
                "mov %1, %%rbp\n\t"
                "jmp *%2\n\t"
                :
                : "r"(shell_rsp_backup), "r"(shell_rbp_backup), "r"(shell_rip_backup)
                : "memory"
            );
            __builtin_unreachable();
        case DOGEIO_TEXT_PRINT:
            if (arg1 != 0) {
                dogeio_text_print((const char*)arg1);
            }
            return 0;
        default:
            dogeio_text_print("Something went wrong (bad instruction)\n");
            __asm__ volatile("mov %0, %%cr3" :: "r"(shell_cr3_backup));
            __asm__ volatile (
                "mov %0, %%rsp\n\t"
                "mov %1, %%rbp\n\t"
                "jmp *%2\n\t"
                :
                : "r"(shell_rsp_backup), "r"(shell_rbp_backup), "r"(shell_rip_backup)
                : "memory"
            );
            __builtin_unreachable();
    }
}

void init_syscalls(void) {
    uint32_t low, high;

    bsp_cpu_storage.kernel_stack_top = (uint64_t)&temporary_syscall_stack[sizeof(temporary_syscall_stack)];
    bsp_cpu_storage.user_stack_scratch = 0;

    uint64_t struct_addr = (uint64_t)&bsp_cpu_storage;
    uint32_t addr_low  = (uint32_t)(struct_addr & 0xFFFFFFFF);
    uint32_t addr_high = (uint32_t)((struct_addr >> 32) & 0xFFFFFFFF);
    __asm__ volatile("wrmsr" :: "a"(addr_low), "d"(addr_high), "c"(MSR_KERNEL_GS));

    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_EFER));
    __asm__ volatile("wrmsr" :: "a"(low | 1), "d"(high), "c"(MSR_EFER));

    uint64_t entry_addr = (uint64_t)&core_sys_entry;
    uint32_t entry_low  = (uint32_t)entry_addr;
    uint32_t entry_high = (uint32_t)(entry_addr >> 32);
    __asm__ volatile("wrmsr" :: "a"(entry_low), "d"(entry_high), "c"(MSR_LSTAR));

    __asm__ volatile("wrmsr" :: "a"(0x200), "d"(0), "c"(MSR_SFMASK));

    /* STAR layout: [63:48] = kernel CS, [47:32] = user CS,
       [31:16] = kernel SS, [15:0] = user SS. */
    uint64_t star_value = ((uint64_t)0x08 << 48) |
                          ((uint64_t)0x1B << 32) |
                          ((uint64_t)0x10 << 16) |
                          0x23ULL;
    __asm__ volatile("wrmsr" :: "a"((uint32_t)(star_value & 0xFFFFFFFFULL)),
                              "d"((uint32_t)(star_value >> 32)), "c"(MSR_STAR));
}
