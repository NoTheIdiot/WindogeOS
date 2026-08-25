#include <stdint.h>
#include <core.h>
#include <basicutil.h>
#include <system.h>
#include <dogeio.h>

static uint8_t kernel_stack[16384] __attribute__((aligned(16)));
void *kernel_stack_top = &kernel_stack[16384];
void *user_rsp_scratch;

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" :: "a"(low), "d"(high), "c"(msr) : "memory");
}

void init_syscall(void) {
    uint64_t efer = rdmsr(0xC0000080);
    wrmsr(0xC0000080, efer | 1);

    uint64_t star = ((uint64_t)0x0010 << 48) | ((uint64_t)0x0008 << 32);
    wrmsr(0xC0000081, star);

    wrmsr(0xC0000082, (uint64_t)syscall_entry);

    wrmsr(0xC0000084, 0x200);
}

uint64_t c_syscall_handler(uint64_t sys_id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    switch (sys_id) {
        case DOGEIO_TEXT_PUTCHAR:
            dogeio_text_putchar((char)arg1, (uint32_t)arg2, (uint32_t)arg3);
            return 0;
        case DOGEIO_TEXT_CLEAR:
            dogeio_text_clear();
            return 0;
        case DOGEIO_TEXT_PRINTCHAR:
            dogeio_text_printchar((char)arg1);
            return 0;
        case DOGEIO_TEXT_PRINT:
            dogeio_text_print((const char *)arg1);
            return 0;
        case DOGEIO_TEXT_PRINTLN:
            dogeio_text_println((const char *)arg1);
            return 0;
        case DOGEIO_TEXT_PRINT_AT:
            dogeio_text_print_at((const char *)arg1, (uint32_t)arg2, (uint32_t)arg3, (uint32_t)arg4);
            return 0;
        case DOGEIO_TEXT_INPUT:
            dogeio_text_input((const char *)arg1, (char *)arg2, (size_t)arg3);
            return 0;
        case DOGEIO_TEXT_COLOR_CHANGE:
            dogeio_text_color_change((uint32_t)arg1);
            return 0;
        case DOGEIO_TEXT_BACKGROUND_CHANGE:
            dogeio_text_background_change((uint32_t)arg1);
            return 0;
        case DOGEIO_TEXT_CLEAR_RAW:
            dogeio_text_clear_raw();
            return 0;

        case DOGEIO_FS_FORMAT:
            return (uint64_t)fs_format();
        case DOGEIO_FS_CREATE:
            return (uint64_t)fs_create((char *)arg1);
        case DOGEIO_FS_MKDIR:
            return (uint64_t)fs_mkdir((char *)arg1);
        case DOGEIO_FS_EXISTS:
            return (uint64_t)fs_exists((char *)arg1);
        case DOGEIO_FS_DELETE:
            return (uint64_t)fs_delete((char *)arg1);
        case DOGEIO_FS_DELETE_LAST_LINE:
            return (uint64_t)fs_delete_last_line((char *)arg1);
        case DOGEIO_FS_READ:
            return (uint64_t)fs_read((char *)arg1, (char *)arg2, (uint32_t)arg3);
        case DOGEIO_FS_WRITE:
            return (uint64_t)fs_write((char *)arg1, (char *)arg2);
        case DOGEIO_FS_LIST_DIR:
            return (uint64_t)fs_list_dir((int)arg1);
        case DOGEIO_FS_RENAME:
            return (uint64_t)fs_rename((char *)arg1, (char *)arg2);
        case DOGEIO_FS_COPY:
            fs_copy((char *)arg1, (char *)arg2);
            return 0;
        case DOGEIO_FS_CHDIR:
            return (uint64_t)fs_chdir((char *)arg1);
        case DOGEIO_FS_DIRNAME:
            return (uint64_t)(uintptr_t)fs_dirname();
        case DOGEIO_FS_MOUNT:
            return (uint64_t)fs_mount();

        case DOGEIO_EXEC_FLAT_BINARY:
            return (uint64_t)exec_flat_binary((const char *)arg1, (int)arg2, (char **)arg3);
        
        case 60:
            __asm__ volatile (
                "mov $0x10, %%ax\n\t"
                "mov %%ax, %%ds\n\t"
                "mov %%ax, %%es\n\t"
                "mov %%ax, %%ss\n\t"

                "movq kernel_shell_rbp(%%rip), %%rbp\n\t"
                "movq kernel_shell_rsp(%%rip), %%rsp\n\t"

                "movq %0, %%rax\n\t"
                "jmp app_return_point\n\t"
                :
                : "r"(arg1)
                : "rax", "memory"
            );
            __builtin_unreachable();

        default:
            return (uint64_t)-1;
    }
}
__attribute__((naked)) void syscall_entry(void) {
    __asm__ volatile (
        "movq %%rsp, user_rsp_scratch(%%rip)\n\t"
        "movq kernel_stack_top(%%rip), %%rsp\n\t"

        "pushq user_rsp_scratch(%%rip)\n\t"
        "pushq %%r11\n\t"
        "pushq %%rcx\n\t"
        "pushq %%rbp\n\t"
        "pushq %%rbx\n\t"
        "subq $8, %%rsp\n\t"

        "movq %%r10, %%r8\n\t"
        "movq %%rdx, %%rcx\n\t"
        "movq %%rsi, %%rdx\n\t"
        "movq %%rdi, %%rsi\n\t"
        "movq %%rax, %%rdi\n\t"

        "call c_syscall_handler\n\t"

        "addq $8, %%rsp\n\t"
        "popq %%rbx\n\t"
        "popq %%rbp\n\t"
        "popq %%rcx\n\t"
        "popq %%r11\n\t"
        "popq %%rsp\n\t"

        "sysretq\n\t"
        ::: "memory"
    );
}

void core_to_user(void *user_entry, void *user_stack_top) {
    uint64_t user_ds = 0x1B;
    uint64_t user_cs = 0x23;

    __asm__ volatile (
        "cli\n\t"
        "mov %w0, %%ds\n\t"
        "mov %w0, %%es\n\t"
        "mov %w0, %%fs\n\t"
        "mov %w0, %%gs\n\t"

        "pushq %0\n\t"
        "pushq %1\n\t"
        "pushq $0x202\n\t"
        "pushq %2\n\t"
        "pushq %3\n\t"
        "iretq\n\t"
        :
        : "r"(user_ds), "r"(user_stack_top), "r"(user_cs), "r"(user_entry)
        : "memory"
    );
}