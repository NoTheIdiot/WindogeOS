#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <bool.h>
#include <core.h>
#include <basicutil.h>
#include <boot/limine.h>

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
#define USER_ADDRESS_LIMIT 0x0000800000000000ULL
#define USER_IO_LIMIT 65536
#define USER_STRING_LIMIT 4096
#define USER_PATH_LIMIT 256
#define USER_INPUT_LIMIT 256

extern void syscall_entry(void);
extern void syscall_exit_to_launcher(uint64_t exit_code) __attribute__((noreturn));
extern volatile struct limine_hhdm_request hhdm_request;

typedef struct {
    uint64_t kernel_rsp;
    uint64_t user_rsp_scratch;
} __attribute__((packed)) per_cpu_data_t;

static uint8_t syscall_stack[16384] __attribute__((aligned(16)));
static per_cpu_data_t bsp_cpu_data;
uint64_t kernel_program_launcher_rsp = 0;
uint64_t kernel_program_launcher_rflags = 0;

struct cpu_regs {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t user_rsp;
} __attribute__((packed));

static bool user_page_accessible(uint64_t address, bool write_access) {
    if (address >= USER_ADDRESS_LIMIT || hhdm_request.response == NULL) {
        return false;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *table = (uint64_t *)((read_cr3() & ~0xFFFULL) + hhdm_offset);
    const size_t indices[] = {
        (address >> 39) & 0x1FF,
        (address >> 30) & 0x1FF,
        (address >> 21) & 0x1FF,
        (address >> 12) & 0x1FF
    };

    for (size_t level = 0; level < 4; level++) {
        uint64_t entry = table[indices[level]];
        if ((entry & 0x5) != 0x5 || (write_access && !(entry & 0x2))) {
            return false;
        }

        if ((level == 1 || level == 2) && (entry & (1ULL << 7))) {
            return true;
        }

        if (level < 3) {
            table = (uint64_t *)((entry & ~0xFFFULL) + hhdm_offset);
        }
    }

    return true;
}

static bool user_range_accessible(uint64_t address, uint64_t length, bool write_access) {
    if (length == 0) {
        return address < USER_ADDRESS_LIMIT;
    }
    if (address == 0 || address >= USER_ADDRESS_LIMIT ||
        length - 1 > (USER_ADDRESS_LIMIT - 1) - address) {
        return false;
    }

    uint64_t last_address = address + length - 1;
    uint64_t page = address & ~((uint64_t)PAGE_SIZE - 1);
    uint64_t last_page = last_address & ~((uint64_t)PAGE_SIZE - 1);

    while (true) {
        if (!user_page_accessible(page, write_access)) {
            return false;
        }
        if (page == last_page) {
            return true;
        }
        page += PAGE_SIZE;
    }
}

static bool copy_user_string(uint64_t address, char *buffer, size_t capacity,
                             size_t *string_length) {
    if (buffer == NULL || capacity == 0 || address == 0) {
        return false;
    }

    for (size_t i = 0; i < capacity; i++) {
        if (address >= USER_ADDRESS_LIMIT || i >= USER_ADDRESS_LIMIT - address ||
            !user_range_accessible(address + i, 1, false)) {
            return false;
        }

        buffer[i] = ((const char *)address)[i];
        if (buffer[i] == '\0') {
            if (string_length != NULL) {
                *string_length = i;
            }
            return true;
        }
    }

    return false;
}

static bool user_string_length(uint64_t address, size_t capacity, size_t *string_length) {
    if (address == 0 || string_length == NULL) {
        return false;
    }

    for (size_t i = 0; i < capacity; i++) {
        if (address >= USER_ADDRESS_LIMIT || i >= USER_ADDRESS_LIMIT - address ||
            !user_range_accessible(address + i, 1, false)) {
            return false;
        }

        if (((const char *)address)[i] == '\0') {
            *string_length = i;
            return true;
        }
    }

    return false;
}

uint64_t syscall_handler(struct cpu_regs *regs) {
    uint64_t sc_num = regs->rax;
    uint64_t ret_val = 0;

    switch (sc_num) {
        case SYS_EXIT: {
            if (kernel_program_launcher_rsp == 0) {
                return (uint64_t)-1;
            }
            cleanup_user_pages();
            syscall_exit_to_launcher(regs->rdi);
        }

        case READ_FILE: {
            char filepath[USER_PATH_LIMIT];
            uint64_t size = regs->rdx;

            if (!copy_user_string(regs->rdi, filepath, sizeof(filepath), NULL) ||
                size > USER_IO_LIMIT ||
                (size != 0 && !user_range_accessible(regs->rsi, size, true))) {
                return (uint64_t)-1;
            }

            if (size == 0) {
                return 0;
            }

            ret_val = (uint64_t)(int64_t)fs_read(filepath, (char *)regs->rsi, size);
            break;
        }

        case WRITE_FILE: {
            char filepath[USER_PATH_LIMIT];
            size_t buffer_length;

            if (!copy_user_string(regs->rdi, filepath, sizeof(filepath), NULL) ||
                !user_string_length(regs->rsi, USER_IO_LIMIT + 1, &buffer_length) ||
                buffer_length > USER_IO_LIMIT) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_write(filepath, (char *)regs->rsi);
            break;
        }
        
        case CREATE_FILE: {
            char filename[USER_PATH_LIMIT];

            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_create(filename);
            break;
        }

        case DELETE_FILE: {
            char filename[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_delete(filename);
            break;
        }

        case CREATE_DIR: {
            char filename[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_mkdir(filename);
            break;
        }

        case RENAME_FILE: {
            char filename[USER_PATH_LIMIT];
            char new_name[USER_PATH_LIMIT];

            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL)) {
                return (uint64_t)-1;
            }

            if (!copy_user_string(regs->rsi, new_name, sizeof(new_name), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_rename(filename, new_name);
            break;
        }

        case FILE_EXISTS: {
            char filename[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_exists(filename);
            break;
        }

        case CHANGE_DIR: {
            char path[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, path, sizeof(path), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_chdir(path);
            break;
        }

        case DELETE_LAST_LINE: {
            char filename[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_delete_last_line(filename);
            break;
        }

        case COPY_FILE: {
            char source[USER_PATH_LIMIT];
            char destination[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, source, sizeof(source), NULL) ||
                !copy_user_string(regs->rsi, destination, sizeof(destination), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_copy(source, destination);
            break;
        }
        
        case PRINT: {
            char text[USER_STRING_LIMIT];

            if (!copy_user_string(regs->rdi, text, sizeof(text), NULL)) {
                return (uint64_t)-1;
            }
            
            dogeio_text_print(text);
            ret_val = 1;
            break;
        }
        
        case PRINTLN: {
            char text[USER_STRING_LIMIT];

            if (!copy_user_string(regs->rdi, text, sizeof(text), NULL)) {
                return (uint64_t)-1;
            }
            
            dogeio_text_println(text);
            ret_val = 1;
            break;
        }

        case CLEAR: {
            log("[syscall] CLEAR entered");
            dogeio_text_clear();
            log("[syscall] CLEAR returned");
            ret_val = 1;
            break;
        }

        case INPUT: {
            char prompt[USER_STRING_LIMIT];
            uint64_t capacity = regs->rdx;
            if (!copy_user_string(regs->rdi, prompt, sizeof(prompt), NULL) ||
                capacity == 0 || capacity > USER_INPUT_LIMIT ||
                !user_range_accessible(regs->rsi, capacity, true)) {
                return (uint64_t)-1;
            }

            dogeio_text_input(prompt, (char *)regs->rsi, (size_t)capacity);
            size_t input_length;
            if (!user_string_length(regs->rsi, capacity, &input_length)) {
                return (uint64_t)-1;
            }
            ret_val = input_length;
            break;
        }

        case GET_KEY:
            ret_val = dogeio_get_key();
            break;

        case PRINT_AT: {
            char text[USER_STRING_LIMIT];
            uint32_t x = (uint32_t)regs->rsi;
            uint32_t y = (uint32_t)regs->rdx;
            uint32_t color = (uint32_t)regs->r10;
            if (x >= TERMINAL_COLS || y >= TERMINAL_ROWS ||
                !copy_user_string(regs->rdi, text, sizeof(text), NULL)) {
                return (uint64_t)-1;
            }

            dogeio_text_print_at(text, x, y, color);
            ret_val = 1;
            break;
        }

        case TEXT_COLOR:
            dogeio_text_color_change((uint32_t)regs->rdi);
            ret_val = 1;
            break;

        case BACKGROUND_COLOR:
            dogeio_text_background_change((uint32_t)regs->rdi);
            ret_val = 1;
            break;

        case SHELL: {
            char text[USER_STRING_LIMIT];
            if (!copy_user_string(regs->rdi, text, sizeof(text), NULL)) {
                return (uint64_t)-1;
            }
            ret_val = (uint64_t)(int64_t)system(text);
            break;
        }

        case EXEC: {
            char filename[USER_STRING_LIMIT];
            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL)) {
                return (uint64_t)-1;
            }
            if (!fs_exists(filename)) {
                ret_val = (uint64_t)-1;
                break;
            }
            system_run_bin(filename, MAX_FLAT_BINARY_SIZE);
            ret_val = 1;
            break;
        }

        default:
            // for some reason a signature for unknown syscall
            duolog("[error] unknown syscall attempted to execute.");
            ret_val = (uint64_t)0xFFFF;
            break;
    }

    return ret_val;
}

void init_syscalls(void) {
    wrmsr(MSR_IA32_EFER, rdmsr(MSR_IA32_EFER) | EFER_SCE);

    uint64_t kernel_cs = 0x08; 
    uint64_t user_base = 0x13;
    
    uint64_t star = (kernel_cs << 32) | (user_base << 48);
    wrmsr(MSR_IA32_STAR, star);

    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_IA32_FMASK, RFLAGS_IF | RFLAGS_TF | RFLAGS_DF);

    bsp_cpu_data.kernel_rsp = (uint64_t)&syscall_stack[sizeof(syscall_stack)];
    bsp_cpu_data.user_rsp_scratch = 0;

    wrmsr(MSR_IA32_KERNEL_GS_BASE, (uint64_t)&bsp_cpu_data);
}
