#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <bool.h>
#include <syscalls.h>

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

#define USER_CODE_BASE           0x0000000000400000ULL
#define USER_STACK_BASE          0x00007FFFF0000000ULL
#define USER_ADDRESS_LIMIT       0x0000800000000000ULL
#define USER_LIMIT               0x0000800000000000ULL

#define USER_IO_LIMIT            65536
#define USER_STRING_LIMIT        4096
#define USER_PATH_LIMIT          256
#define USER_INPUT_LIMIT         256

static bool is_valid_user_ptr(const void *ptr, size_t len) {
    uintptr_t addr = (uintptr_t)ptr;
    return addr != 0 && addr < USER_LIMIT && len <= (USER_LIMIT - addr);
}

static bool copy_str_from_user(char *dst, const char *user_src, size_t max_len) {
    if (!is_valid_user_ptr(user_src, 1) || max_len == 0) return false;
    for (size_t i = 0; i < max_len; i++) {
        if (!is_valid_user_ptr(&user_src[i], 1)) return false;
        dst[i] = user_src[i];
        if (dst[i] == '\0') return true;
    }
    dst[max_len - 1] = '\0';
    return false;
}

static bool copy_from_user(void *dst, const void *user_src, size_t len) {
    if (!is_valid_user_ptr(user_src, len)) return false;
    memcpy(dst, user_src, len);
    return true;
}

static bool copy_to_user(void *user_dst, const void *src, size_t len) {
    if (!is_valid_user_ptr(user_dst, len)) return false;
    memcpy(user_dst, src, len);
    return true;
}

extern void syscall_entry(void);

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
            sprintln("[SYSCALL] Peter. The horse doesn't have to be here.");
            cleanup_pages();
            if (kernel_program_launcher_rsp == 0) {
                sprintln("[SYSCALL] ERROR: kernel_program_launcher_rsp is NULL again...");
                halt();
            }
            syscall_exit_to_launcher();
            ret_val = 0;
            break;
        }

        case ReadFile: {
            const char *user_path = (const char *)regs->rdi;
            char *user_buf        = (char *)regs->rsi;
            size_t len            = (size_t)regs->rdx;

            if (len == 0 || len > USER_IO_LIMIT || !is_valid_user_ptr(user_buf, len)) {
                ret_val = (uint64_t)-1;
                break;
            }

            char kpath[USER_PATH_LIMIT];
            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                ret_val = (uint64_t)-1;
                break;
            }

            static uint8_t io_bounce[USER_IO_LIMIT];
            memset(io_bounce, 0, len);

            int bytes_read = fs_read(kpath, (char *)io_bounce, len);
            if (bytes_read < 0) {
                ret_val = (uint64_t)-1;
                break;
            }

            size_t copy_len = (size_t)bytes_read;
            if (copy_len > len) copy_len = len;

            if (!copy_to_user(user_buf, io_bounce, copy_len)) {
                ret_val = (uint64_t)-1;
                break;
            }

            ret_val = (uint64_t)bytes_read;
            break;
        }

        case WriteFile: {
            const char *user_path = (const char *)regs->rdi;
            const char *user_buf  = (const char *)regs->rsi;
            size_t len            = (size_t)regs->rdx;

            if (len == 0 || len > USER_IO_LIMIT || !is_valid_user_ptr(user_buf, len)) {
                ret_val = (uint64_t)-1;
                break;
            }

            char kpath[USER_PATH_LIMIT];
            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                ret_val = (uint64_t)-1;
                break;
            }

            static uint8_t io_bounce[USER_IO_LIMIT];
            if (!copy_from_user(io_bounce, user_buf, len)) {
                ret_val = (uint64_t)-1;
                break;
            }

            ret_val = (uint64_t)fs_write(kpath, (char *)io_bounce, len);
            break;
        }

        case CreateFile: {
            const char *user_path = (const char *)regs->rdi;
            char kpath[USER_PATH_LIMIT];

            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                ret_val = (uint64_t)-1;
                break;
            }

            ret_val = (uint64_t)fs_create(kpath);
            break;
        }

        case CreateDir: {
            const char *user_path = (const char *)regs->rdi;
            char kpath[USER_PATH_LIMIT];

            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                ret_val = (uint64_t)-1;
                break;
            }

            ret_val = (uint64_t)fs_mkdir(kpath);
            break;
        }

        case DeleteThing: {
            const char *user_path = (const char *)regs->rdi;
            char kpath[USER_PATH_LIMIT];

            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                ret_val = (uint64_t)-1;
                break;
            }
            ret_val = (uint64_t)fs_delete(kpath);
            break;
        }

        case Exists: {
            const char *user_path = (const char *)regs->rdi;
            char kpath[USER_PATH_LIMIT];

            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                ret_val = (uint64_t)-1;
                break;
            }
            ret_val = (uint64_t)fs_exists(kpath);
            break;
        }

        case ListDirectory: {
            const char *user_path = (const char *)regs->rdi;
            fs_t *user_buf        = (fs_t *)regs->rsi;
            int max               = (int)regs->rdx;

            if (max <= 0 || (size_t)max > (USER_IO_LIMIT / sizeof(fs_t))) {
                ret_val = (uint64_t)-1;
                break;
            }

            size_t total_bytes = (size_t)max * sizeof(fs_t);
            if (!is_valid_user_ptr(user_buf, total_bytes)) {
                ret_val = (uint64_t)-1;
                break;
            }

            char kpath[USER_PATH_LIMIT];
            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                ret_val = (uint64_t)-1;
                break;
            }

            static uint8_t io_bounce[USER_IO_LIMIT];
            memset(io_bounce, 0, total_bytes);

            int count = fs_list(kpath, (fs_t *)io_bounce, max);
            if (count < 0) {
                ret_val = (uint64_t)-1;
                break;
            }

            if (count > max) count = max;
            size_t copy_bytes = (size_t)count * sizeof(fs_t);

            if (copy_bytes > 0 && !copy_to_user(user_buf, io_bounce, copy_bytes)) {
                ret_val = (uint64_t)-1;
                break;
            }

            ret_val = (uint64_t)count;
            break;
        }

        case ChangeDirectory: {
            const char *user_path = (const char *)regs->rdi;
            char kpath[USER_PATH_LIMIT];

            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                ret_val = (uint64_t)-1;
                break;
            }
            ret_val = (uint64_t)fs_chdir(kpath);
            break;
        }

        case CurrentDirectory: {
            char *user_buf = (char *)regs->rdi;
            size_t len     = (size_t)regs->rsi;

            if (len == 0 || len > USER_PATH_LIMIT || !is_valid_user_ptr(user_buf, len)) {
                ret_val = (uint64_t)-1;
                break;
            }

            char cwd[len];
            fs_get_dir(cwd, len);
            if (!cwd) {
                ret_val = (uint64_t)-1;
                break;
            }

            size_t cwd_len = strlen(cwd) + 1;
            if (cwd_len > len) {
                ret_val = (uint64_t)-1;
                break;
            }

            if (!copy_to_user(user_buf, cwd, cwd_len)) {
                ret_val = (uint64_t)-1;
                break;
            }

            ret_val = (uint64_t)(cwd_len - 1);
            break;
        }

        case Print: {
            const char *user_str = (const char *)regs->rdi;
            char kbuf[USER_STRING_LIMIT];

            if (!copy_str_from_user(kbuf, user_str, sizeof(kbuf))) {
                ret_val = (uint64_t)-1;
                break;
            }

            print(kbuf);
            ret_val = 0;
            break;
        }

        case Input: {
            const char *user_prompt = (const char *)regs->rdi;
            char *user_buf          = (char *)regs->rsi;
            int size                = (int)regs->rdx;

            if (size <= 0 || size > USER_INPUT_LIMIT || !is_valid_user_ptr(user_buf, (size_t)size)) {
                ret_val = (uint64_t)-1;
                break;
            }

            char kprompt[USER_INPUT_LIMIT];
            kprompt[0] = '\0';

            if (user_prompt != NULL) {
                if (!copy_str_from_user(kprompt, user_prompt, sizeof(kprompt))) {
                    ret_val = (uint64_t)-1;
                    break;
                }
            }


            static char kinput[1024];
            memset(kinput, 0, sizeof(kinput));

            input(kprompt, kinput);
            kinput[size - 1] = '\0';

            size_t out_len = strlen(kinput) + 1;
            if (out_len > (size_t)size) out_len = (size_t)size;

            if (!copy_to_user(user_buf, kinput, out_len)) {
                ret_val = (uint64_t)-1;
                break;
            }

            ret_val = (uint64_t)(out_len - 1);
            break;
        }

        case Shutdown: {
            shutdown();
            ret_val = 0;
            break; 
        }

        case Reboot: {
            reboot();
            ret_val = 0;
            break;
        }

        case Username: {
            char* username = "root";
            ret_val = (uint64_t)username;
            break;
        }

        default: {
            print("[\033[91mNot Wow\033[0m] Unknown system call tried to execute, but doesn't have id. ");
            sprint("Unknown system call tried to execute, but doesn't have id. ");
            char buffer[24];
            itoa((int)sc_num, buffer);
            println(buffer);
            sprintln(buffer);
            ret_val = (uint64_t)-1;
            break;
        }
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
        (void)basic_features;
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
