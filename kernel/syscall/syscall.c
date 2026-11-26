#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <syscalls.h>
#include <bool.h>
#include <io.h>

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
#define USER_START               0x0000000010000000ULL

#define USER_IO_LIMIT            65536
#define USER_STRING_LIMIT        4096
#define USER_PATH_LIMIT          256
#define USER_INPUT_LIMIT         256
#define MAX_EXEC_ARGS            32
#define MAX_ARG_LEN              256
#define NESTED_SYSCALL_SLICE     16384ULL

#ifndef ERR_UNKNOWN_SYSCALL
#define ERR_UNKNOWN_SYSCALL      ((uint64_t)-1)
#endif
#ifndef ERR_UNKNOWN_OPCODE
#define ERR_UNKNOWN_OPCODE       ((uint64_t)-2)
#endif
#ifndef ERR_BAD_PTR
#define ERR_BAD_PTR              ((uint64_t)-3)
#endif
#ifndef ERR_INVAL
#define ERR_INVAL                ((uint64_t)-4)
#endif

typedef struct {
    uint64_t size;
} doge_fs_stat_t;

uint64_t heap_pointer = USER_START;

extern mount_t exfat_vol;
extern uint64_t kernel_program_launcher_rsp;
extern uint64_t kernel_program_launcher_rflags;
__attribute__((noreturn)) extern void syscall_exit_to_launcher(void);
extern void syscall_entry(void);

extern void sti(void);
extern void halt(void);

typedef struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t r11;
    uint64_t rcx;
} __attribute__((packed)) registers_t;

typedef struct {
    uint64_t kernel_rsp;
    uint64_t user_rsp_scratch;
} __attribute__((packed)) per_cpu_data_t;

static uint8_t syscall_stack[65536] __attribute__((aligned(16)));
per_cpu_data_t bsp_cpu_data;
static bool cpu_supports_nx;
static bool cpu_supports_rdrand;

#define IPC_MSG_MAX 256
typedef struct {
    bool has_msg;
    uint64_t sender_tag;
    size_t len;
    uint8_t data[IPC_MSG_MAX];
} ipc_slot_t;

static ipc_slot_t g_ipc_slot = {0};

static inline uint64_t current_rsp_u64(void) {
    uint64_t sp = 0;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(sp));
    return sp;
}

static inline void refresh_kernel_gs_base(void) {
    uint64_t gs_base = (uint64_t)&bsp_cpu_data;
    wrmsr(MSR_IA32_KERNEL_GS_BASE, gs_base);
}

static bool is_valid_user_ptr(const void *ptr, size_t len) {
    uintptr_t addr = (uintptr_t)ptr;
    if (addr == 0 || addr >= USER_LIMIT) return false;
    if (len > USER_LIMIT - addr) return false;
    return true;
}

static bool copy_str_from_user(char *dst, const char *user_src, size_t max_len) {
    if (!dst || !user_src || max_len == 0) return false;
    for (size_t i = 0; i < max_len; i++) {
        if (!is_valid_user_ptr(&user_src[i], 1)) {
            dst[0] = '\0';
            return false;
        }
        char c = user_src[i];
        dst[i] = c;
        if (c == '\0') return true;
    }
    dst[max_len - 1] = '\0';
    return false;
}

static bool copy_from_user(void *dst, const void *user_src, size_t len) {
    if (len == 0) return true;
    if (!dst || !user_src || !is_valid_user_ptr(user_src, len)) return false;
    memcpy(dst, user_src, len);
    return true;
}

static bool copy_to_user(void *user_dst, const void *src, size_t len) {
    if (len == 0) return true;
    if (!user_dst || !src || !is_valid_user_ptr(user_dst, len)) return false;
    memcpy(user_dst, src, len);
    return true;
}

static void unmap_and_free_range(uint64_t start, uint64_t end) {
    for (uint64_t va = start; va < end; va += PAGE_SIZE) {
        uintptr_t phys = unmap_page((uintptr_t)va);
        if (phys) {
            free_page(phys);
        }
    }
}

static inline bool arch_rdrand64(uint64_t *out) {
    uint64_t val = 0;
    uint8_t ok = 0;
    __asm__ volatile (
        "rdrand %0; setc %1"
        : "=r"(val), "=qm"(ok)
    );
    if (!ok) return false;
    *out = val;
    return true;
}

static inline void reset_syscall_run_state(void) {
    heap_pointer = USER_START;
    bsp_cpu_data.kernel_rsp = (uint64_t)&syscall_stack[sizeof(syscall_stack)];
    bsp_cpu_data.user_rsp_scratch = 0;
    refresh_kernel_gs_base();
}

static inline uint64_t handle_text(registers_t *regs) {
    uint64_t op = regs->rdi;

    switch (op) {
        case TEXT_PRINTF: {
            const char *user_str = (const char *)regs->rsi;
            if (!is_valid_user_ptr(user_str, 1)) {
                return ERR_BAD_PTR;
            }

            size_t explicit_len = (size_t)regs->rdx;
            bool use_explicit = (explicit_len > 0 && explicit_len <= USER_IO_LIMIT);
            size_t max_bytes = use_explicit ? explicit_len : USER_IO_LIMIT;

            char kbuf[256];
            size_t total_printed = 0;
            size_t offset = 0;

            while (offset < max_bytes) {
                size_t chunk = max_bytes - offset;
                if (chunk > sizeof(kbuf) - 1) {
                    chunk = sizeof(kbuf) - 1;
                }

                size_t i = 0;
                bool hit_null = false;

                for (; i < chunk; i++) {
                    char ch;
                    if (!copy_from_user(&ch, user_str + offset + i, 1)) {
                        return ERR_BAD_PTR;
                    }
                    if (ch == '\0') {
                        hit_null = true;
                        break;
                    }
                    kbuf[i] = ch;
                }

                kbuf[i] = '\0';

                if (i > 0) {
                    printf("%s", kbuf);
                    total_printed += i;
                }

                if (hit_null) {
                    break;
                }

                offset += i;
            }

            return total_printed;
        }

        case TEXT_GETKEY: {
            sti();
            char c = 0;
            while ((c = get_key()) == 0) {
                __asm__ volatile ("pause");
            }
            return (uint64_t)(uint8_t)c;
        }

        case TEXT_INPUT: {
            sti();
            char *user_buf = (char *)regs->rsi;
            size_t max_len = (size_t)regs->rdx;

            if (max_len == 0 || max_len > USER_INPUT_LIMIT) {
                return ERR_INVAL;
            }
            if (!is_valid_user_ptr(user_buf, max_len)) {
                return ERR_BAD_PTR;
            }

            char kbuf[USER_INPUT_LIMIT];
            size_t pos = 0;

            while (pos + 1 < max_len) {
                char c = get_key();
                if (c == 0) {
                    __asm__ volatile ("pause");
                    continue;
                }

                if (c == '\r' || c == '\n') {
                    putchar('\n');
                    break;
                } else if (c == '\b' || c == 0x7F) {
                    if (pos > 0) {
                        pos--;
                        printf("\b \b");
                    }
                } else {
                    kbuf[pos++] = c;
                    putchar(c);
                }
            }

            kbuf[pos] = '\0';
            if (!copy_to_user(user_buf, kbuf, pos + 1)) {
                return ERR_BAD_PTR;
            }

            return (uint64_t)pos;
        }

        default: {
            return ERR_UNKNOWN_OPCODE;
        }
    }
}

static inline uint64_t handle_doge(registers_t *regs) {
    uint64_t op = regs->rdi;

    switch (op) {
        case DOGE_EXIT: {
            sprintf("[SYSCALL] exiting\n");
            cleanup_pages();
            reset_syscall_run_state();
            heap_pointer = USER_START;
            if (kernel_program_launcher_rsp == 0) {
                sprintf("[SYSCALL] ERROR: kernel_program_launcher_rsp is NULL\n");
                halt();
            }
            syscall_exit_to_launcher();
            return 0;
        }

        case DOGE_SLEEP: {
            sti();
            uint64_t time = regs->rsi;
            sleep_ms(time);
            return 0;
        }

        case DOGE_UPTIME: {
            update_time();
            return get_epoch_seconds();
        }

        case DOGE_GET_TIME: {
            char *user_buf = (char *)regs->rsi;
            size_t buf_size = (size_t)regs->rdx;

            update_time();
            const char *t = get_time();
            if (!t) t = "00:00:00";

            size_t len = strlen(t);
            if (!user_buf || buf_size <= len || !is_valid_user_ptr(user_buf, len + 1)) {
                return ERR_BAD_PTR;
            }
            if (!copy_to_user(user_buf, t, len + 1)) {
                return ERR_BAD_PTR;
            }
            return (uint64_t)len;
        }

        case DOGE_BEEP: {
            uint64_t hz   = regs->rsi;
            uint64_t time = regs->rdx;
            speaker_beep((uint32_t)hz, (uint32_t)time);
            return 0;
        }

        case DOGE_RAND: {
            uint64_t *user_out = (uint64_t *)regs->rsi;
            if (!user_out || !is_valid_user_ptr(user_out, sizeof(uint64_t))) {
                return ERR_BAD_PTR;
            }
            if (!cpu_supports_rdrand) {
                return ERR_UNKNOWN_OPCODE;
            }
            uint64_t r = 0;
            if (!arch_rdrand64(&r)) {
                return ERR_INVAL;
            }
            if (!copy_to_user(user_out, &r, sizeof(r))) {
                return ERR_BAD_PTR;
            }
            return 0;
        }

        case DOGE_REBOOT: {
            reboot();
            return 0;
        }

        case DOGE_SHUTDOWN: {
            shutdown();
            return 0;
        }

        default: {
            return ERR_UNKNOWN_OPCODE;
        }
    }
}

static inline uint64_t handle_fs(registers_t *regs) {
    uint64_t op = regs->rdi;

    switch (op) {
        case FS_READ: {
            const char *user_filename = (const char *)regs->rsi;
            uint8_t *user_buffer = (uint8_t *)regs->rdx;
            uint64_t max_size = regs->r10;

            char kfilename[USER_PATH_LIMIT];
            if (!copy_str_from_user(kfilename, user_filename, sizeof(kfilename))) {
                return ERR_BAD_PTR;
            }

            if (max_size == 0 || max_size > USER_IO_LIMIT || !is_valid_user_ptr(user_buffer, max_size)) {
                return ERR_BAD_PTR;
            }

            static uint8_t kbuf[USER_IO_LIMIT];
            uint64_t bytes_read = 0;

            int res = read_file(&exfat_vol, kfilename, kbuf, max_size, &bytes_read);
            if (res != 0) {
                return ERR_INVAL;
            }

            if (bytes_read > 0 && !copy_to_user(user_buffer, kbuf, bytes_read)) {
                return ERR_BAD_PTR;
            }

            return bytes_read;
        }

        case FS_WRITE: {
            const char *user_filename = (const char *)regs->rsi;
            const uint8_t *user_buffer = (const uint8_t *)regs->rdx;
            uint64_t size = regs->r10;

            char kfilename[USER_PATH_LIMIT];
            if (!copy_str_from_user(kfilename, user_filename, sizeof(kfilename))) {
                return ERR_BAD_PTR;
            }

            if (size > USER_IO_LIMIT || (size > 0 && !is_valid_user_ptr(user_buffer, size))) {
                return ERR_BAD_PTR;
            }

            static uint8_t kbuf[USER_IO_LIMIT];
            if (size > 0 && !copy_from_user(kbuf, user_buffer, size)) {
                return ERR_BAD_PTR;
            }

            int res = write_file(&exfat_vol, kfilename, kbuf, size);
            return (res == 0) ? size : ERR_INVAL;
        }

        case FS_CREATE: {
            const char *user_filename = (const char *)regs->rsi;

            char kfilename[USER_PATH_LIMIT];
            if (!copy_str_from_user(kfilename, user_filename, sizeof(kfilename))) {
                return ERR_BAD_PTR;
            }

            int res = create_file(&exfat_vol, kfilename);
            return (res == 0) ? 0 : ERR_INVAL;
        }

        case FS_DELETE: {
            const char *user_filename = (const char *)regs->rsi;

            char kfilename[USER_PATH_LIMIT];
            if (!copy_str_from_user(kfilename, user_filename, sizeof(kfilename))) {
                return ERR_BAD_PTR;
            }

            int res = delete_file(&exfat_vol, kfilename);
            return (res == 0) ? 0 : ERR_INVAL;
        }

        case FS_STAT: {
            const char *user_filename = (const char *)regs->rsi;
            doge_fs_stat_t *user_stat = (doge_fs_stat_t *)regs->rdx;

            char kfilename[USER_PATH_LIMIT];
            if (!copy_str_from_user(kfilename, user_filename, sizeof(kfilename))) {
                return ERR_BAD_PTR;
            }
            if (!user_stat || !is_valid_user_ptr(user_stat, sizeof(doge_fs_stat_t))) {
                return ERR_BAD_PTR;
            }

            static uint8_t probe[USER_IO_LIMIT];
            uint64_t bytes_read = 0;
            int res = read_file(&exfat_vol, kfilename, probe, sizeof(probe), &bytes_read);
            if (res != 0) {
                return ERR_INVAL;
            }

            doge_fs_stat_t st = { .size = bytes_read };
            if (!copy_to_user(user_stat, &st, sizeof(st))) {
                return ERR_BAD_PTR;
            }
            return 0;
        }

        case FS_LISTDIR: {
            const char *user_path = (const char *)regs->rsi;
            fs_t *user_entries = (fs_t *)regs->rdx;
            uint64_t max_entries = regs->r10;

            char kpath[USER_PATH_LIMIT];

            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                return ERR_BAD_PTR;
            }

            if (max_entries == 0 || max_entries > 256) {
                return ERR_INVAL;
            }

            if (!is_valid_user_ptr(user_entries, sizeof(fs_t) * max_entries)) {
                return ERR_BAD_PTR;
            }

            fs_t kentries[256];

            int count = fs_list(kpath, kentries, (int)max_entries);
            if (count < 0) {
                return ERR_INVAL;
            }

            if (count > 0) {
                if (!copy_to_user(user_entries, kentries, sizeof(fs_t) * count)) {
                    return ERR_BAD_PTR;
                }
            }

            return (uint64_t)count;
        }

        case FS_CHDIR: {
            const char *user_path = (const char *)regs->rsi;

            char kpath[USER_PATH_LIMIT];

            if (!copy_str_from_user(kpath, user_path, sizeof(kpath))) {
                return ERR_BAD_PTR;
            }

            int res = fs_chdir(kpath);

            return (res == 0) ? 0 : ERR_INVAL;
        }

        case FS_GETDIR: {
            char *user_buf = (char *)regs->rsi;
            uint64_t size = regs->rdx;

            if (!user_buf || size == 0 || size > USER_PATH_LIMIT) {
                return ERR_INVAL;
            }

            if (!is_valid_user_ptr(user_buf, size)) {
                return ERR_BAD_PTR;
            }

            char cwd[USER_PATH_LIMIT];
            get_cwd(cwd, sizeof(cwd));

            size_t len = strlen(cwd) + 1;

            if (len > size) {
                return ERR_INVAL;
            }

            if (!copy_to_user(user_buf, cwd, len)) {
                return ERR_BAD_PTR;
            }

            return len - 1;
        }

        default: {
            return ERR_UNKNOWN_OPCODE;
        }
    }
}

static inline uint64_t handle_mem(registers_t *regs) {
    uint64_t op = regs->rdi;

    switch (op) {
        case MEM_SBRK: {
            int64_t increment = (int64_t)regs->rsi;
            uint64_t old_brk = heap_pointer;

            if (increment == 0) {
                return old_brk;
            }

            if (increment < 0) {
                uint64_t dec = (uint64_t)(-(increment + 1)) + 1;
                if (heap_pointer < USER_START || dec > heap_pointer - USER_START) {
                    return ERR_INVAL;
                }
                heap_pointer -= dec;
                return old_brk;
            }

            uint64_t inc = (uint64_t)increment;
            if (inc > USER_LIMIT - heap_pointer) {
                return ERR_INVAL;
            }

            uint64_t new_brk = heap_pointer + inc;
            uint64_t page_start = ALIGN_DOWN(old_brk, PAGE_SIZE);
            uint64_t page_end = ALIGN_UP(new_brk, PAGE_SIZE);
            uint64_t flags = PAGE_USER_FLAGS | (cpu_supports_nx ? PAGE_NO_EXECUTE : 0);

            for (uint64_t va = page_start; va < page_end; va += PAGE_SIZE) {
                uint64_t phys = alloc_page();
                if (!phys) {
                    unmap_and_free_range(page_start, va);
                    return ERR_INVAL;
                }
                if (!map_page((uintptr_t)va, (uintptr_t)phys, flags)) {
                    free_page((uintptr_t)phys);
                    unmap_and_free_range(page_start, va);
                    return ERR_INVAL;
                }
                memset((void *)(uintptr_t)va, 0, PAGE_SIZE);
            }

            heap_pointer = new_brk;
            return old_brk;
        }

        case MEM_MAP: {
            uint64_t virt = regs->rsi;
            uint64_t size = regs->rdx;
            uint64_t user_flags = regs->r10;

            if (size == 0 || size > (16ULL * 1024 * 1024)) {
                return ERR_INVAL;
            }

            size_t aligned_size = (size_t)ALIGN_UP(size, PAGE_SIZE);
            if (virt == 0) {
                uint64_t base = ALIGN_UP(heap_pointer, PAGE_SIZE);
                if (aligned_size > USER_LIMIT - base) {
                    return ERR_INVAL;
                }
                virt = base;
                heap_pointer = base + aligned_size;
            } else {
                if ((virt % PAGE_SIZE) != 0 || !is_valid_user_ptr((const void *)(uintptr_t)virt, aligned_size)) {
                    return ERR_BAD_PTR;
                }
            }

            uint64_t pte_flags = PAGE_PRESENT | PAGE_USER;
            if (user_flags & PF_W) {
                pte_flags |= PAGE_WRITABLE;
            }
            if (!(user_flags & PF_X) && cpu_supports_nx) {
                pte_flags |= PAGE_NO_EXECUTE;
            }

            for (size_t off = 0; off < aligned_size; off += PAGE_SIZE) {
                uint64_t phys = alloc_page();
                if (!phys) {
                    unmap_and_free_range(virt, virt + off);
                    return ERR_INVAL;
                }
                if (!map_page((uintptr_t)(virt + off), (uintptr_t)phys, pte_flags)) {
                    free_page((uintptr_t)phys);
                    unmap_and_free_range(virt, virt + off);
                    return ERR_INVAL;
                }
                memset((void *)(uintptr_t)(virt + off), 0, PAGE_SIZE);
            }

            return virt;
        }

        default: {
            return ERR_UNKNOWN_OPCODE;
        }
    }
}

static inline uint64_t handle_ipc(registers_t *regs) {
    uint64_t op = regs->rdi;

    switch (op) {
        case 1: {
            const void *user_buf = (const void *)regs->rsi;
            size_t len = (size_t)regs->rdx;

            if (len == 0 || len > IPC_MSG_MAX) {
                return ERR_INVAL;
            }
            if (!copy_from_user(g_ipc_slot.data, user_buf, len)) {
                return ERR_BAD_PTR;
            }
            g_ipc_slot.len = len;
            g_ipc_slot.has_msg = true;
            return (uint64_t)len;
        }

        case 2: {
            void *user_buf = (void *)regs->rsi;
            size_t max_len = (size_t)regs->rdx;

            if (!g_ipc_slot.has_msg) {
                return 0;
            }
            if (max_len == 0 || !is_valid_user_ptr(user_buf, max_len)) {
                return ERR_BAD_PTR;
            }

            size_t to_copy = (g_ipc_slot.len < max_len) ? g_ipc_slot.len : max_len;
            if (!copy_to_user(user_buf, g_ipc_slot.data, to_copy)) {
                return ERR_BAD_PTR;
            }
            g_ipc_slot.has_msg = false;
            return (uint64_t)to_copy;
        }

        default: {
            return ERR_UNKNOWN_OPCODE;
        }
    }
}

static inline uint64_t handle_proc(registers_t *regs) {
    uint64_t op = regs->rdi;

    switch (op) {
        case 1: {
            char kfilename[USER_PATH_LIMIT];
            if (!copy_str_from_user(kfilename, (const char *)regs->rsi, sizeof(kfilename))) {
                return ERR_BAD_PTR;
            }

            size_t file_size = (size_t)regs->rdx;
            /* x86_64 `syscall` clobbers RCX with return RIP; read argc from r10/r9 if passed there */
            int argc = (int)regs->r10;
            const char *const *user_argv = (const char *const *)regs->r8;

            static char kargv_storage[MAX_EXEC_ARGS][MAX_ARG_LEN];
            static char *kargv_ptrs[MAX_EXEC_ARGS + 1];

            if (argc < 0 || argc > MAX_EXEC_ARGS) {
                return ERR_INVAL;
            }

            if (argc > 0) {
                if (!user_argv || !is_valid_user_ptr(user_argv, (size_t)argc * sizeof(char *))) {
                    return ERR_BAD_PTR;
                }
                for (int i = 0; i < argc; i++) {
                    const char *uarg = NULL;
                    if (!copy_from_user(&uarg, &user_argv[i], sizeof(uarg))) {
                        return ERR_BAD_PTR;
                    }
                    if (!uarg) {
                        argc = i;
                        break;
                    }
                    if (!copy_str_from_user(kargv_storage[i], uarg, MAX_ARG_LEN)) {
                        return ERR_BAD_PTR;
                    }
                    kargv_ptrs[i] = kargv_storage[i];
                }
            }
            kargv_ptrs[argc] = NULL;

            sprintf("[SYSCALL] PROC spawn: file=%s size=%llu argc=%d argv=%p\n",
                    kfilename, (unsigned long long)file_size, argc, (void *)user_argv);

            uint64_t saved_kernel_rsp = bsp_cpu_data.kernel_rsp;
            uint64_t saved_user_scratch = bsp_cpu_data.user_rsp_scratch;
            uint64_t saved_heap = heap_pointer;

            uint64_t stack_bottom = (uint64_t)&syscall_stack[0];
            uint64_t sp_now = current_rsp_u64();
            uint64_t child_rsp = (sp_now - 256ULL) & ~0xFULL;

            if (child_rsp > stack_bottom + 4096ULL && child_rsp <= saved_kernel_rsp) {
                bsp_cpu_data.kernel_rsp = child_rsp;
            } else if (saved_kernel_rsp > stack_bottom + NESTED_SYSCALL_SLICE) {
                bsp_cpu_data.kernel_rsp = (saved_kernel_rsp - NESTED_SYSCALL_SLICE) & ~0xFULL;
            }

            heap_pointer = USER_START;
            refresh_kernel_gs_base();

            run(kfilename, file_size, argc, (argc > 0) ? kargv_ptrs : NULL);

            bsp_cpu_data.kernel_rsp = saved_kernel_rsp;
            bsp_cpu_data.user_rsp_scratch = saved_user_scratch;
            heap_pointer = saved_heap;
            refresh_kernel_gs_base();
            return 0;
        }

        case 2: {
            sprintf("[SYSCALL] PROC exit\n");
            cleanup_pages();
            reset_syscall_run_state();
            heap_pointer = USER_START;
            if (kernel_program_launcher_rsp == 0) {
                halt();
            }
            syscall_exit_to_launcher();
            return 0;
        }

        default: {
            return ERR_UNKNOWN_OPCODE;
        }
    }
}

uint64_t syscall_handler(registers_t *regs) {
    sprintf("[SYSCALL] rax=%llu rdi=%llu rsi=%llu\n", regs->rax, regs->rdi, regs->rsi);
    switch (regs->rax) {
        case TEXT: {
            return handle_text(regs);
        }

        case DOGE: {
            return handle_doge(regs);
        }

        case FS: {
            return handle_fs(regs);
        }

        case MEM: {
            return handle_mem(regs);
        }

        case IPC: {
            return handle_ipc(regs);
        }

        case PROC: {
            return handle_proc(regs);
        }

        default: {
            printf("Unknown syscall attempted to execute: %llu\n", regs->rax);
            return ERR_UNKNOWN_SYSCALL;
        }
    }
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
        cpu_supports_nx = (cpuid_edx & (1U << 20)) != 0;
    }

    uint64_t efer = rdmsr(MSR_IA32_EFER) | EFER_SCE;
    if (cpu_supports_nx) {
        efer |= EFER_NXE;
    }
    wrmsr(MSR_IA32_EFER, efer);

    uint64_t kernel_cs = 0x08;
    uint64_t user_base = 0x10;

    uint64_t star = (kernel_cs << 32) | (user_base << 48);
    wrmsr(MSR_IA32_STAR, star);

    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_IA32_FMASK, RFLAGS_IF | RFLAGS_TF | RFLAGS_DF);

    bsp_cpu_data.kernel_rsp = (uint64_t)&syscall_stack[sizeof(syscall_stack)];
    bsp_cpu_data.user_rsp_scratch = 0;

    refresh_kernel_gs_base();

    uint64_t gs_base = (uint64_t)&bsp_cpu_data;
    uint64_t readback_gs = rdmsr(MSR_IA32_KERNEL_GS_BASE);
    sprintf("[SYSCALL] init: lstar=0x%llx kernel_rsp=0x%llx gs_base=0x%llx readback=0x%llx nx=%d rdrand=%d\n",
            (uint64_t)syscall_entry,
            bsp_cpu_data.kernel_rsp,
            gs_base,
            readback_gs,
            cpu_supports_nx ? 1 : 0,
            cpu_supports_rdrand ? 1 : 0);
}