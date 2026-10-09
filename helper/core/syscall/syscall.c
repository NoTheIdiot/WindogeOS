#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <bool.h>
#include <core.h>
#include <basicutil.h>
#include <boot/limine.h>
#include <boot/syscall_internal.h>
#include <boot/syscall.h>
#include <time.h>

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
#define PAGE_SIZE       4096
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
static bool linux_syscall_abi_active;
static bool cpu_supports_nx;
static bool cpu_supports_rdrand;

static bool try_rdrand(uint64_t *value) __attribute__((target("rdrnd")));

static bool try_rdrand(uint64_t *value) {
    unsigned char success;
    __asm__ volatile ("rdrand %0; setc %1"
                      : "=r"(*value), "=qm"(success)
                      :
                      : "cc");
    return success != 0;
}

static bool valid_user_memory_range(uint64_t address, uint64_t length,
                                    uint64_t *rounded_length) {
    if (address == 0 || address >= USER_ADDRESS_LIMIT || length == 0 ||
        length > USER_MEMORY_MAX_SIZE ||
        (address & ((uint64_t)PAGE_SIZE - 1)) != 0 ||
        length > USER_ADDRESS_LIMIT - address) {
        return false;
    }

    uint64_t rounded = (length + (uint64_t)PAGE_SIZE - 1) &
                       ~((uint64_t)PAGE_SIZE - 1);
    if (rounded < length || rounded > USER_ADDRESS_LIMIT - address) {
        return false;
    }
    *rounded_length = rounded;
    return true;
}

bool user_page_accessible(uint64_t address, bool write_access) {
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

bool user_range_accessible(uint64_t address, uint64_t length, bool write_access) {
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

bool copy_user_string(uint64_t address, char *buffer, size_t capacity,
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

bool user_string_length(uint64_t address, size_t capacity, size_t *string_length) {
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

static uint64_t native_user_memory_map(uint64_t address, uint64_t length,
                                       uint64_t permissions) {
    uint64_t rounded_length;
    if (hhdm_request.response == NULL ||
        (permissions & ~7ULL) != 0 ||
        !valid_user_memory_range(address, length, &rounded_length)) {
        return (uint64_t)-1;
    }

    for (uint64_t page = address; page < address + rounded_length;
         page += PAGE_SIZE) {
        if (user_page_mapped(page)) {
            return (uint64_t)-1;
        }
    }

    for (uint64_t page = address; page < address + rounded_length;
         page += PAGE_SIZE) {
        map_user_page(page, pmm_alloc_zeroed_page());
        if (!protect_user_page(
                page, permissions != 0,
                (permissions & USER_MEMORY_WRITE) != 0,
                (permissions & USER_MEMORY_EXEC) != 0)) {
            for (uint64_t rollback = address; rollback <= page;
                 rollback += PAGE_SIZE) {
                (void)unmap_user_page(rollback);
            }
            return (uint64_t)-1;
        }
    }
    return address;
}

static uint64_t native_user_memory_protect(uint64_t address, uint64_t length,
                                           uint64_t permissions) {
    uint64_t rounded_length;
    if ((permissions & ~7ULL) != 0 ||
        !valid_user_memory_range(address, length, &rounded_length)) {
        return (uint64_t)-1;
    }

    for (uint64_t page = address; page < address + rounded_length;
         page += PAGE_SIZE) {
        if (!user_page_mapped(page)) {
            return (uint64_t)-1;
        }
    }
    for (uint64_t page = address; page < address + rounded_length;
         page += PAGE_SIZE) {
        if (!protect_user_page(
                page, permissions != 0,
                (permissions & USER_MEMORY_WRITE) != 0,
                (permissions & USER_MEMORY_EXEC) != 0)) {
            return (uint64_t)-1;
        }
    }
    return 0;
}

static uint64_t native_user_memory_unmap(uint64_t address, uint64_t length) {
    uint64_t rounded_length;
    if (!valid_user_memory_range(address, length, &rounded_length)) {
        return (uint64_t)-1;
    }
    for (uint64_t page = address; page < address + rounded_length;
         page += PAGE_SIZE) {
        if (!user_page_mapped(page)) {
            return (uint64_t)-1;
        }
    }
    for (uint64_t page = address; page < address + rounded_length;
         page += PAGE_SIZE) {
        if (!unmap_user_page(page)) {
            return (uint64_t)-1;
        }
    }
    return 0;
}

typedef struct {
    uint64_t rsp;
    uint64_t rflags;
} launcher_context_t;

static launcher_context_t save_launcher_context(void) {
    return (launcher_context_t) {
        .rsp = kernel_program_launcher_rsp,
        .rflags = kernel_program_launcher_rflags
    };
}

static void restore_launcher_context(launcher_context_t context) {
    kernel_program_launcher_rsp = context.rsp;
    kernel_program_launcher_rflags = context.rflags;
}

static void copy_uname_field(char destination[65], const char *source) {
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    size_t i = 0;
    while (i < 64 && source[i] != '\0') {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

static uint64_t run_flat_binary(char *filename) {
    uint64_t parent_pages[256];
    if (!suspend_user_pages(parent_pages)) {
        return (uint64_t)-1;
    }
    launcher_context_t parent = save_launcher_context();
    system_run_bin(filename, MAX_FLAT_BINARY_SIZE);
    if (!restore_user_pages(parent_pages)) {
        return (uint64_t)-1;
    }
    restore_launcher_context(parent);
    return 1;
}

static uint64_t run_flat_binary_args(char *filename, int argc,
                                     char **argv) {
    uint64_t parent_pages[256];
    if (!suspend_user_pages(parent_pages)) {
        return (uint64_t)-1;
    }
    launcher_context_t parent = save_launcher_context();
    system_run_bin_args(filename, MAX_FLAT_BINARY_SIZE, argc, argv);
    if (!restore_user_pages(parent_pages)) {
        return (uint64_t)-1;
    }
    restore_launcher_context(parent);
    return 1;
}

static uint64_t run_elf_binary(char *filename) {
    uint64_t file_size;
    bool is_dir;
    if (fs_get_info(filename, &file_size, &is_dir) != 0 ||
        is_dir || file_size == 0 || file_size > MAX_FLAT_BINARY_SIZE) {
        return (uint64_t)-1;
    }

    uint64_t parent_pages[256];
    if (!suspend_user_pages(parent_pages)) {
        return (uint64_t)-1;
    }
    launcher_context_t parent = save_launcher_context();
    bool parent_linux_abi = linux_syscall_abi_active;
    int result = system_run_elf(filename, file_size);
    syscall_set_linux_abi(parent_linux_abi);
    if (!restore_user_pages(parent_pages)) {
        return (uint64_t)-1;
    }
    restore_launcher_context(parent);
    return (uint64_t)(int64_t)result;
}

static uint64_t native_random_bytes(uint64_t address, uint64_t length) {
    if (length == 0) {
        return 0;
    }
    if (!cpu_supports_rdrand || length > USER_IO_LIMIT ||
        !user_range_accessible(address, length, true)) {
        return (uint64_t)-1;
    }

    uint8_t *output = (uint8_t *)address;
    uint64_t written = 0;
    while (written < length) {
        uint64_t random_value = 0;
        bool generated = false;
        for (size_t attempt = 0; attempt < 10; attempt++) {
            if (try_rdrand(&random_value)) {
                generated = true;
                break;
            }
        }
        if (!generated) {
            return written == 0 ? (uint64_t)-1 : written;
        }

        uint64_t chunk_size = length - written;
        if (chunk_size > sizeof(random_value)) {
            chunk_size = sizeof(random_value);
        }
        memcpy(output + written, &random_value, (size_t)chunk_size);
        written += chunk_size;
    }

    return written;
}

void syscall_set_linux_abi(bool enabled) {
    linux_syscall_abi_active = enabled;
    linux_syscall_reset();
}

uint64_t syscall_handler(syscall_registers_t *regs) {
    if (linux_syscall_abi_active) {
        return linux_syscall_handler(regs);
    }

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

        case READ_FILE_AT: {
            char filepath[USER_PATH_LIMIT];
            uint64_t size = regs->r10;

            if (!copy_user_string(regs->rdi, filepath, sizeof(filepath), NULL) ||
                size > USER_IO_LIMIT ||
                (size != 0 && !user_range_accessible(regs->rsi, size, true))) {
                return (uint64_t)-1;
            }

            if (size == 0) {
                return 0;
            }

            ret_val = (uint64_t)(int64_t)fs_read_raw_at(
                filepath, (uint8_t *)regs->rsi, regs->rdx, size);
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

        case APPEND_FILE: {
            char filepath[USER_PATH_LIMIT];
            uint64_t size = regs->rdx;

            if (!copy_user_string(regs->rdi, filepath, sizeof(filepath), NULL) ||
                size > USER_IO_LIMIT ||
                (size != 0 && !user_range_accessible(regs->rsi, size, false))) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_append_data(
                filepath, (const uint8_t *)regs->rsi, (uint32_t)size);
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

        case GET_CWD: {
            const char *cwd = fs_dirname();
            uint64_t capacity = regs->rsi;
            if (cwd == NULL || capacity == 0) {
                return (uint64_t)-1;
            }

            size_t cwd_length = str_strlen(cwd);
            if ((uint64_t)cwd_length >= capacity ||
                !user_range_accessible(regs->rdi, (uint64_t)cwd_length + 1, true)) {
                return (uint64_t)-1;
            }

            memcpy((char *)regs->rdi, cwd, cwd_length + 1);
            ret_val = (uint64_t)cwd_length;
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

        case MOVE_FILE: {
            char source[USER_PATH_LIMIT];
            char destination[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, source, sizeof(source), NULL) ||
                !copy_user_string(regs->rsi, destination, sizeof(destination), NULL)) {
                return (uint64_t)-1;
            }

            ret_val = (uint64_t)(int64_t)fs_move(source, destination);
            break;
        }

        case MAP_USER_MEMORY:
            return native_user_memory_map(regs->rdi, regs->rsi, regs->rdx);

        case PROTECT_USER_MEMORY:
            return native_user_memory_protect(regs->rdi, regs->rsi, regs->rdx);

        case UNMAP_USER_MEMORY:
            return native_user_memory_unmap(regs->rdi, regs->rsi);

        case GET_PID:
            return 1;

        case GET_UID:
        case GET_GID:
        case GET_EUID:
        case GET_EGID:
            return 0;

        case GET_UNAME: {
            if (regs->rdi == 0 ||
                !user_range_accessible(regs->rdi, sizeof(windoge_utsname_t), true)) {
                return (uint64_t)-1;
            }

            windoge_utsname_t uname;
            memset(&uname, 0, sizeof(uname));
            copy_uname_field(uname.sysname, "WindogeOS");
            copy_uname_field(uname.nodename,
                             computer_name[0] != '\0' ? computer_name : "windoge");
            copy_uname_field(uname.release, windoge_version);
            copy_uname_field(uname.version, windoge_version);
            copy_uname_field(uname.machine, "x86_64");
            memcpy((void *)regs->rdi, &uname, sizeof(uname));
            return 0;
        }

        case RANDOM_BYTES:
            if (regs->rdx != 0) {
                return (uint64_t)-1;
            }
            return native_random_bytes(regs->rdi, regs->rsi);

        case GET_USER_NAME: {
            uint64_t capacity = regs->rsi;
            size_t username_length = 0;
            while (username_length < sizeof(current_user) &&
                   current_user[username_length] != '\0') {
                username_length++;
            }
            if (username_length == sizeof(current_user)) {
                return (uint64_t)-1;
            }
            if (capacity <= username_length ||
                !user_range_accessible(regs->rdi, username_length + 1, true)) {
                return (uint64_t)-1;
            }
            memcpy((char *)regs->rdi, current_user, username_length + 1);
            return (uint64_t)username_length;
        }

        case EXEC_ARGS: {
            char filename[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL) ||
                regs->rsi > 32 ||
                (regs->rsi != 0 &&
                 !user_range_accessible(regs->rdx, regs->rsi * sizeof(uint64_t), false))) {
                return (uint64_t)-1;
            }
            if (!fs_exists(filename)) {
                return (uint64_t)-1;
            }

            char argument_storage[32][256];
            char *arguments[32];
            int argc = (int)regs->rsi;
            for (int i = 0; i < argc; i++) {
                uint64_t argument_address;
                memcpy(&argument_address,
                       (const void *)(regs->rdx + (uint64_t)i * sizeof(uint64_t)),
                       sizeof(argument_address));
                if (!copy_user_string(argument_address, argument_storage[i],
                                      sizeof(argument_storage[i]), NULL)) {
                    return (uint64_t)-1;
                }
                arguments[i] = argument_storage[i];
            }

            return run_flat_binary_args(filename, argc, arguments);
        }

        case EXEC_ELF: {
            char filename[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, filename, sizeof(filename), NULL)) {
                return (uint64_t)-1;
            }
            return run_elf_binary(filename);
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

        case MKDIR_RECURSIVE: {
            char path[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, path, sizeof(path), NULL)) {
                return (uint64_t)-1;
            }

            char *cursor = path;
            while (*cursor != '\0') {
                if (*cursor == '/') {
                    char saved = *cursor;
                    *cursor = '\0';
                    if (path[0] != '\0' && !fs_exists(path)) {
                        if (fs_mkdir(path) != 0) {
                            *cursor = saved;
                            return (uint64_t)-1;
                        }
                    }
                    *cursor = saved;
                }
                cursor++;
            }

            ret_val = (uint64_t)(int64_t)fs_mkdir(path);
            break;
        }

        case GET_TIME: {
            if (regs->rdi == 0 || !user_range_accessible(regs->rdi, sizeof(uint64_t), true)) {
                return (uint64_t)-1;
            }
            *(uint64_t *)regs->rdi = time_get_epoch_seconds();
            ret_val = 0;
            break;
        }

        case STAT: {
            char path[USER_PATH_LIMIT];
            if (!copy_user_string(regs->rdi, path, sizeof(path), NULL) ||
                regs->rsi == 0 || !user_range_accessible(regs->rsi, sizeof(dogec_stat_t), true)) {
                return (uint64_t)-1;
            }

            dogec_stat_t *stat_out = (dogec_stat_t *)regs->rsi;
            memset(stat_out, 0, sizeof(*stat_out));
            stat_out->exists = fs_exists(path) ? 1ULL : 0ULL;
            if (stat_out->exists) {
                stat_out->size = 0;
                stat_out->is_dir = 0;
            }
            ret_val = stat_out->exists ? 0ULL : (uint64_t)-1;
            break;
        }

        case SHELL: {
            char text[USER_STRING_LIMIT];
            if (!copy_user_string(regs->rdi, text, sizeof(text), NULL)) {
                return (uint64_t)-1;
            }
            uint64_t parent_pages[256];
            if (!suspend_user_pages(parent_pages)) {
                return (uint64_t)-1;
            }
            launcher_context_t parent = save_launcher_context();
            ret_val = (uint64_t)(int64_t)system(text);
            if (!restore_user_pages(parent_pages)) {
                return (uint64_t)-1;
            }
            restore_launcher_context(parent);
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
            ret_val = run_flat_binary(filename);
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
    uint64_t user_base = 0x13;
    
    uint64_t star = (kernel_cs << 32) | (user_base << 48);
    wrmsr(MSR_IA32_STAR, star);

    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_IA32_FMASK, RFLAGS_IF | RFLAGS_TF | RFLAGS_DF);

    bsp_cpu_data.kernel_rsp = (uint64_t)&syscall_stack[sizeof(syscall_stack)];
    bsp_cpu_data.user_rsp_scratch = 0;

    wrmsr(MSR_IA32_KERNEL_GS_BASE, (uint64_t)&bsp_cpu_data);
}
