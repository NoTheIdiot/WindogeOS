#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <system.h>
#include <limine.h>

#define ELF_CLASS_64        2
#define ELF_DATA_LSB        1
#define ELF_VERSION_CURRENT 1
#define ELF_TYPE_EXEC       2
#define ELF_MACHINE_X86_64  62

#define PT_LOAD             1

#define USER_STACK_TOP        0x00007FFFFFFFF000ULL
#define USER_STACK_PAGE_COUNT 4
#define USER_STACK_SIZE       (PAGE_SIZE * USER_STACK_PAGE_COUNT)
#define USER_STACK_BASE       (USER_STACK_TOP - USER_STACK_SIZE)

#define KERNEL_STACK_SIZE     (PAGE_SIZE * 4)

// Per-CPU structure for handling syscall stack swapping via GS MSRs
typedef struct __attribute__((packed)) {
    uint64_t kernel_stack;       // Offset 0: Kernel RSP loaded on syscall
    uint64_t user_stack_scratch; // Offset 8: Temporary storage for user RSP
} cpu_gs_t;

// Static allocation for bootstrap CPU (or replace with your kernel's per-CPU block)
static cpu_gs_t bootstrap_cpu_gs;
__attribute__((noreturn)) extern void syscall_exit_to_launcher(void);

// Allocate a dedicated static backing buffer for the bootstrap kernel stack
static uint8_t bootstrap_kernel_stack_space[KERNEL_STACK_SIZE];

typedef struct __attribute__((packed)) {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr;

typedef struct __attribute__((packed)) {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr;

extern volatile struct limine_hhdm_request hhdm_request;

extern void to_userland_ring3_args(
    uint64_t entry,
    uint64_t stack,
    uint64_t arg0,
    uint64_t arg1
);

static uint64_t align_down_page(uint64_t value)
{
    return value & ~(PAGE_SIZE - 1);
}

static uint64_t align_up_page(uint64_t value)
{
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

int run_elf(char *filename, uint64_t size)
{
    sprintln("[ELF] === START RUN_ELF ===");
    sprintln("[ELF] Starting program loader");

    if (!filename) {
        sprintln("[ELF] ERROR: Invalid filename (NULL)");
        return -1;
    }

    sprintln("[ELF] Checking HHDM response...");
    if (!hhdm_request.response) {
        sprintln("[ELF] ERROR: No HHDM response");
        return -1;
    }
    sprintln("[ELF] HHDM response validated successfully.");

    elf64_ehdr ehdr;
    sprintln("[ELF] Reading ELF header from disk/filesystem...");

    if (fs_read_raw_at(
            filename,
            (uint8_t *)&ehdr,
            0,
            sizeof(ehdr))
        != (int)sizeof(ehdr))
    {
        sprintln("[ELF] ERROR: Failed to read ELF header");
        return -2;
    }
    sprintln("[ELF] ELF header read successfully.");

    sprintln("[ELF] Verifying ELF magic bytes...");
    if (ehdr.e_ident[0] != 0x7F ||
        ehdr.e_ident[1] != 'E'  ||
        ehdr.e_ident[2] != 'L'  ||
        ehdr.e_ident[3] != 'F')
    {
        sprintln("[ELF] ERROR: Invalid ELF magic");
        return -3;
    }
    sprintln("[ELF] ELF magic matches 0x7F 'E' 'L' 'F'.");

    if (ehdr.e_ident[4] != ELF_CLASS_64) {
        sprintln("[ELF] ERROR: Not ELF64");
        return -3;
    }
    sprintln("[ELF] Verified class: ELF64.");

    if (ehdr.e_ident[5] != ELF_DATA_LSB) {
        sprintln("[ELF] ERROR: Unsupported endian");
        return -3;
    }
    sprintln("[ELF] Verified endian: Little-Endian.");

    if (ehdr.e_version != ELF_VERSION_CURRENT) {
        sprintln("[ELF] ERROR: Unsupported ELF version");
        return -3;
    }

    if (ehdr.e_type != ELF_TYPE_EXEC) {
        sprintln("[ELF] ERROR: Not executable");
        return -3;
    }
    sprintln("[ELF] Verified type: Executable binary.");

    if (ehdr.e_machine != ELF_MACHINE_X86_64) {
        sprintln("[ELF] ERROR: Not x86_64");
        return -3;
    }
    sprintln("[ELF] Verified architecture: x86_64.");

    if (ehdr.e_phentsize != sizeof(elf64_phdr)) {
        sprintln("[ELF] ERROR: Invalid PHDR size");
        return -3;
    }

    if (ehdr.e_phnum == 0) {
        sprintln("[ELF] ERROR: No program headers");
        return -3;
    }

    sprintln("[ELF] Mapping PT_LOAD segments...");

    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        elf64_phdr phdr;

        uint64_t phdr_offset =
            ehdr.e_phoff +
            ((uint64_t)i * ehdr.e_phentsize);

        sprintln("[ELF] Reading program header entry...");
        if (fs_read_raw_at(
                filename,
                (uint8_t *)&phdr,
                phdr_offset,
                sizeof(phdr))
            != (int)sizeof(phdr))
        {
            sprintln("[ELF] ERROR: Failed to read PHDR");
            return -4;
        }

        if (phdr.p_type != PT_LOAD) {
            sprintln("[ELF] Skipping non-PT_LOAD program header.");
            continue;
        }

        if (phdr.p_memsz == 0) {
            sprintln("[ELF] Skipping segment with zero memory size.");
            continue;
        }

        if (phdr.p_memsz < phdr.p_filesz) {
            sprintln("[ELF] ERROR: Corrupt segment (memsz < filesz)");
            return -4;
        }

        if (phdr.p_memsz > UINT64_MAX - phdr.p_vaddr) {
            sprintln("[ELF] ERROR: Segment overflow detected");
            return -4;
        }

        if (phdr.p_vaddr >= USER_STACK_BASE) {
            sprintln("[ELF] ERROR: Segment overlaps stack base");
            return -4;
        }

        uint64_t page_start =
            align_down_page(phdr.p_vaddr);

        uint64_t page_end =
            align_up_page(
                phdr.p_vaddr +
                phdr.p_memsz);

        sprintln("[ELF] Processing pages for PT_LOAD segment...");
        for (uint64_t page_vaddr = page_start;
             page_vaddr < page_end;
             page_vaddr += PAGE_SIZE)
        {
            uint64_t page_phys =
                pmm_alloc_zeroed_page();

            if (!page_phys) {
                sprintln("[ELF] ERROR: Out of physical pages during segment map");
                return -5;
            }

            map_user_page(
                page_vaddr,
                page_phys);

            uint64_t file_start =
                phdr.p_vaddr;

            uint64_t file_end =
                phdr.p_vaddr +
                phdr.p_filesz;

            uint64_t page_limit =
                page_vaddr +
                PAGE_SIZE;

            uint64_t copy_start =
                (page_vaddr > file_start)
                    ? page_vaddr
                    : file_start;

            uint64_t copy_end =
                (page_limit < file_end)
                    ? page_limit
                    : file_end;

            if (copy_start < copy_end)
            {
                uint64_t file_offset =
                    phdr.p_offset +
                    (copy_start - file_start);

                uint64_t copy_size =
                    copy_end - copy_start;

                uint8_t *dest =
                    (uint8_t *)(
                        page_phys +
                        hhdm_request.response->offset +
                        (copy_start - page_vaddr));

                if (fs_read_raw_at(
                        filename,
                        dest,
                        file_offset,
                        copy_size)
                    != (int)copy_size)
                {
                    sprintln("[ELF] ERROR: Segment read/copy failed");
                    return -6;
                }
            }
        }
    }
    sprintln("[ELF] All PT_LOAD segments successfully mapped.");

    sprintln("[ELF] Allocating user stack space...");

    for (uint64_t i = 0;
         i < USER_STACK_PAGE_COUNT;
         i++)
    {
        uint64_t page_phys =
            pmm_alloc_zeroed_page();

        if (!page_phys) {
            sprintln("[ELF] ERROR: Stack allocation failed");
            return -7;
        }

        map_user_page(
            USER_STACK_BASE +
            (i * PAGE_SIZE),
            page_phys);
    }
    sprintln("[ELF] User stack allocated and mapped successfully.");

    uint64_t stack_top =
        USER_STACK_TOP - 16;

    char buf[32];

    sprintln("[ELF] Printing ELF Execution Parameters:");
    sprint("[ELF] Entry = ");
    uint64_to_str(ehdr.e_entry, buf);
    sprintln(buf);

    sprint("[ELF] Stack = ");
    uint64_to_str(stack_top, buf);
    sprintln(buf);

    sprintln("[ELF] Preparing kernel stack and CPU GS base for safe syscall handling...");
    
    // Set kernel_stack to point to the top of our allocated kernel stack buffer (grows downwards)
    bootstrap_cpu_gs.kernel_stack = (uint64_t)(bootstrap_kernel_stack_space + KERNEL_STACK_SIZE);

    sprint("[ELF] Bootstrap Kernel Stack Top assigned at: ");
    uint64_to_str(bootstrap_cpu_gs.kernel_stack, buf);
    sprintln(buf);

    // initialize IA32_KERNEL_GS_BASE (MSR 0xC0000102) so that 
    // syscall entry won't fault when swapping gs and writing to [gs:8].
    // (Ensure bootstrap_cpu_gs.kernel_stack points to your actual kernel stack top)
    uint64_t gs_base = (uint64_t)&bootstrap_cpu_gs;
    __asm__ volatile (
        "wrmsr"
        :
        : "c" (0xC0000102),
          "a" ((uint32_t)(gs_base & 0xFFFFFFFF)),
          "d" ((uint32_t)(gs_base >> 32))
    );
    sprintln("[ELF] IA32_KERNEL_GS_BASE MSR initialized successfully.");

    sprintln("[ELF] Jumping to ring 3 via to_userland_ring3_args...");
    to_userland_ring3_args(
        ehdr.e_entry,
        stack_top,
        0,
        0);

    sprintln("[ELF] ERROR: Returned from userland unexpectedly");

    for (;;)
        __asm__ volatile("hlt");
}