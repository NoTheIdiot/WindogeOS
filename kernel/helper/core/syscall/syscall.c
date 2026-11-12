#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <bool.h>
#include <limine.h>

#define MSR_IA32_STAR           0xC0000081
#define MSR_IA32_LSTAR          0xC0000082
#define MSR_IA32_FMASK          0xC0000084
#define MSR_IA32_KERNEL_GS_BASE 0xC0000102

#define SYS_EXIT                60

#define USER_CODE_BASE     0x0000000000400000ULL
#define USER_STACK_BASE    0x00007FFFF0000000ULL
#define USER_IO_LIMIT      65536
#define USER_STRING_LIMIT  4096
#define USER_PATH_LIMIT    256
#define USER_INPUT_LIMIT   256
#define USER_MEMORY_MAX_SIZE   0x10000000ULL
#define USER_MEMORY_WRITE      (1ULL << 0)
#define USER_MEMORY_EXEC       (1ULL << 1)
#define MAX_FLAT_BINARY_SIZE   0x400000ULL

extern void syscall_entry(void);
extern volatile struct limine_hhdm_request hhdm_request;
extern void system_run_bin(char *filename, uint64_t max_size);
extern void system_run_bin_args(char *filename, uint64_t max_size, int argc, char **argv);
extern int system_run_elf(char *filename, uint64_t file_size);
extern uint64_t read_cr3(void);
extern int fs_get_info(char *filename, uint64_t *size, bool *is_dir);

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

extern uint64_t syscall_handler_switch(registers_t *regs, uint64_t sc_num);

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
    if (address == 0 || address >= PAGE_ADDRESS_MASK || length == 0 ||
        length > USER_MEMORY_MAX_SIZE ||
        (address & (PAGE_SIZE - 1)) != 0 ||
        length > PAGE_ADDRESS_MASK - address) {
        return false;
    }

    uint64_t rounded = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (rounded < length || rounded > PAGE_ADDRESS_MASK - address) {
        return false;
    }
    *rounded_length = rounded;
    return true;
}

bool user_page_accessible(uint64_t address, bool write_access) {
    if (address >= PAGE_ADDRESS_MASK || hhdm_request.response == NULL) {
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
        if ((entry & PAGE_USER_FLAGS) != PAGE_USER_FLAGS || (write_access && !(entry & PAGE_WRITABLE))) {
            return false;
        }

        if ((level == 1 || level == 2) && (entry & PAGE_LARGE)) {
            return true;
        }

        if (level < 3) {
            table = (uint64_t *)((entry & PAGE_ADDRESS_MASK) + hhdm_offset);
        }
    }

    return true;
}

bool user_range_accessible(uint64_t address, uint64_t length, bool write_access) {
    if (length == 0) {
        return address < PAGE_ADDRESS_MASK;
    }
    if (address == 0 || address >= PAGE_ADDRESS_MASK ||
        length - 1 > (PAGE_ADDRESS_MASK - 1) - address) {
        return false;
    }

    uint64_t last_address = address + length - 1;
    uint64_t page = address & ~(PAGE_SIZE - 1);
    uint64_t last_page = last_address & ~(PAGE_SIZE - 1);

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
        if (address >= PAGE_ADDRESS_MASK || i >= PAGE_ADDRESS_MASK - address ||
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
        if (address >= PAGE_ADDRESS_MASK || i >= PAGE_ADDRESS_MASK - address ||
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
    int result = system_run_elf(filename, file_size);
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

uint64_t syscall_handler(registers_t *regs) {
    uint64_t sc_num = regs->rax;
    uint64_t ret_val = 0;

    ret_val = syscall_handler_switch(regs, sc_num);

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

    wrmsr(MSR_IA32_EFER, rdmsr(MSR_IA32_EFER) | (1ULL << 0));

    uint64_t kernel_cs = 0x08; 
    uint64_t user_base = 0x10;
    
    uint64_t star = (kernel_cs << 32) | (user_base << 48);
    wrmsr(MSR_IA32_STAR, star);

    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_IA32_FMASK, (1ULL << 8) | (1ULL << 9) | (1ULL << 10));

    bsp_cpu_data.kernel_rsp = (uint64_t)&syscall_stack[sizeof(syscall_stack)];
    bsp_cpu_data.user_rsp_scratch = 0;

    wrmsr(MSR_IA32_KERNEL_GS_BASE, (uint64_t)&bsp_cpu_data);
}