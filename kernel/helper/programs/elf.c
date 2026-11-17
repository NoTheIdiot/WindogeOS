#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <system.h>

extern uintptr_t hhdm_offset;

#define ELF_CLASS_64        2
#define ELF_DATA_LSB        1
#define ELF_VERSION_CURRENT 1
#define ELF_TYPE_EXEC       2
#define ELF_MACHINE_X86_64  62

#define PT_LOAD             1
#define PF_X                0x1
#define PF_W                0x2
#define PF_R                0x4

#define USER_STACK_TOP        0x00007FFFFFFFF000ULL
#define USER_STACK_PAGE_COUNT 4
#define USER_STACK_SIZE       (PAGE_SIZE * USER_STACK_PAGE_COUNT)
#define USER_STACK_BASE       (USER_STACK_TOP - USER_STACK_SIZE)

#define KERNEL_STACK_SIZE     (PAGE_SIZE * 4)

typedef struct __attribute__((packed)) {
    uint64_t kernel_stack;
    uint64_t user_stack_scratch;
} cpu_gs_t;

static cpu_gs_t bootstrap_cpu_gs;
__attribute__((noreturn)) extern void syscall_exit_to_launcher(void);

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

extern void to_userland_ring3_args(
    uint64_t entry,
    uint64_t stack,
    uint64_t arg0,
    uint64_t arg1
);

static uint64_t align_down_page(uint64_t value)
{
    return value & ~(PAGE_SIZE - 1ULL);
}

static uint64_t align_up_page(uint64_t value)
{
    return (value + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
}

#define MAX_VMAS 16

struct vma_region {
    bool active;
    char filename[128];
    uint64_t p_vaddr;
    uint64_t p_memsz;
    uint64_t p_offset;
    uint64_t p_filesz;
    uint64_t pte_flags;
};

static struct vma_region current_vmas[MAX_VMAS];

static void clear_vmas(void) {
    for (int i = 0; i < MAX_VMAS; i++) {
        current_vmas[i].active = false;
    }
}

static bool add_vma(const char *filename, uint64_t vaddr, uint64_t memsz,
                    uint64_t offset, uint64_t filesz, uint64_t flags)
{
    for (int i = 0; i < MAX_VMAS; i++) {
        if (!current_vmas[i].active) {
            current_vmas[i].active = true;

            int j = 0;
            for (; j < 127 && filename[j]; j++) {
                current_vmas[i].filename[j] = filename[j];
            }
            current_vmas[i].filename[j] = '\0';

            current_vmas[i].p_vaddr   = vaddr;
            current_vmas[i].p_memsz   = memsz;
            current_vmas[i].p_offset  = offset;
            current_vmas[i].p_filesz  = filesz;
            current_vmas[i].pte_flags = flags;
            return true;
        }
    }
    return false;
}

static void *get_hhdm_ptr(uint64_t vaddr) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)((cr3 & ~0xFFFULL) + hhdm_offset);

    size_t pml4_i = (vaddr >> 39) & 0x1FF;
    size_t pdpt_i = (vaddr >> 30) & 0x1FF;
    size_t pd_i   = (vaddr >> 21) & 0x1FF;
    size_t pt_i   = (vaddr >> 12) & 0x1FF;

    if (!(pml4[pml4_i] & 1)) return NULL;
    uint64_t *pdpt = (uint64_t *)((pml4[pml4_i] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pdpt[pdpt_i] & 1)) return NULL;
    uint64_t *pd = (uint64_t *)((pdpt[pdpt_i] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pd[pd_i] & 1)) return NULL;
    uint64_t *pt = (uint64_t *)((pd[pd_i] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pt[pt_i] & 1)) return NULL;

    uint64_t paddr = pt[pt_i] & 0x000FFFFFFFFFF000ULL;
    uint64_t offset = vaddr & 0xFFFULL;
    return (void *)(paddr + hhdm_offset + offset);
}

bool try_demand_page(uint64_t fault_addr, uint64_t err_code)
{
    struct vma_region *vma = NULL;
    for (int i = 0; i < MAX_VMAS; i++) {
        if (current_vmas[i].active &&
            fault_addr >= align_down_page(current_vmas[i].p_vaddr) &&
            fault_addr <  align_up_page(current_vmas[i].p_vaddr + current_vmas[i].p_memsz))
        {
            vma = &current_vmas[i];
            break;
        }
    }

    if (!vma) {
        return false;
    }

    if ((err_code & (1ULL << 1)) && !(vma->pte_flags & PTE_WRITABLE)) {
        return false;
    }

    uint64_t page_vaddr = align_down_page(fault_addr);
    uintptr_t page_phys = alloc_page();
    if (!page_phys) {
        return false;
    }

    memset((void *)(page_phys + hhdm_offset), 0, PAGE_SIZE);

    uint64_t file_start = vma->p_vaddr;
    uint64_t file_end   = vma->p_vaddr + vma->p_filesz;
    uint64_t page_limit = page_vaddr + PAGE_SIZE;

    uint64_t copy_start = (page_vaddr > file_start) ? page_vaddr : file_start;
    uint64_t copy_end   = (page_limit < file_end)   ? page_limit : file_end;

    if (copy_start < copy_end) {
        uint64_t file_offset = vma->p_offset + (copy_start - file_start);
        uint64_t copy_size   = copy_end - copy_start;
        uint8_t *dest        = (uint8_t *)(page_phys + hhdm_offset + (copy_start - page_vaddr));

        __asm__ volatile ("sti");
        int bytes_read = fs_read_raw_at(vma->filename, dest, file_offset, copy_size);
        __asm__ volatile ("cli");

        if (bytes_read != (int)copy_size) {
            free_page(page_phys);
            return false;
        }
    }

    if (!map_page(page_vaddr, page_phys, vma->pte_flags)) {
        free_page(page_phys);
        return false;
    }

    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)((cr3 & ~0xFFFULL) + hhdm_offset);

    size_t pml4_i = (page_vaddr >> 39) & 0x1FF;
    size_t pdpt_i = (page_vaddr >> 30) & 0x1FF;
    size_t pd_i   = (page_vaddr >> 21) & 0x1FF;

    pml4[pml4_i] |= (PTE_USER | PTE_WRITABLE);
    uint64_t *pdpt = (uint64_t *)((pml4[pml4_i] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    pdpt[pdpt_i] |= (PTE_USER | PTE_WRITABLE);
    uint64_t *pd = (uint64_t *)((pdpt[pdpt_i] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    pd[pd_i] |= (PTE_USER | PTE_WRITABLE);

    __asm__ volatile ("invlpg (%0)" :: "r"(page_vaddr) : "memory");
    return true;
}

int run_elf(char *filename, uint64_t size, int argc, char **argv)
{
    (void)size;
    char buf[64];

    sprintln("[ELF] === START RUN_ELF ===");
    sprintln("[ELF] Starting program loader");

    if (!filename) {
        sprintln("[ELF] ERROR: Invalid filename (NULL)");
        return -1;
    }

    sprintln("[ELF] Checking HHDM offset...");
    if (!hhdm_offset) {
        sprintln("[ELF] ERROR: No HHDM offset");
        return -1;
    }
    sprintln("[ELF] HHDM offset validated successfully.");

    elf64_ehdr ehdr;
    sprintln("[ELF] Reading ELF header from disk/filesystem...");

    if (fs_read_raw_at(filename, (uint8_t *)&ehdr, 0, sizeof(ehdr)) != (int)sizeof(ehdr)) {
        sprintln("[ELF] ERROR: Failed to read ELF header");
        return -2;
    }
    sprintln("[ELF] ELF header read successfully.");

    if (ehdr.e_ident[0] != 0x7F ||
        ehdr.e_ident[1] != 'E'  ||
        ehdr.e_ident[2] != 'L'  ||
        ehdr.e_ident[3] != 'F')
    {
        sprintln("[ELF] ERROR: Invalid ELF magic");
        return -3;
    }

    if (ehdr.e_ident[4] != ELF_CLASS_64) {
        sprintln("[ELF] ERROR: Not ELF64");
        return -3;
    }

    if (ehdr.e_ident[5] != ELF_DATA_LSB) {
        sprintln("[ELF] ERROR: Unsupported endian");
        return -3;
    }

    if (ehdr.e_version != ELF_VERSION_CURRENT) {
        sprintln("[ELF] ERROR: Unsupported ELF version");
        return -3;
    }

    if (ehdr.e_type != ELF_TYPE_EXEC) {
        sprintln("[ELF] ERROR: Not executable");
        return -3;
    }

    if (ehdr.e_machine != ELF_MACHINE_X86_64) {
        sprintln("[ELF] ERROR: Not x86_64");
        return -3;
    }

    if (ehdr.e_phentsize != sizeof(elf64_phdr) || ehdr.e_phnum == 0) {
        sprintln("[ELF] ERROR: Invalid PHDR size or count");
        return -3;
    }

    clear_vmas();

    sprintln("[ELF] Registering PT_LOAD segments for demand paging...");

    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        elf64_phdr phdr;
        uint64_t phdr_offset = ehdr.e_phoff + ((uint64_t)i * ehdr.e_phentsize);

        if (fs_read_raw_at(filename, (uint8_t *)&phdr, phdr_offset, sizeof(phdr)) != (int)sizeof(phdr)) {
            sprintln("[ELF] ERROR: Failed to read PHDR");
            return -4;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0) {
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

        if (phdr.p_vaddr >= USER_STACK_BASE || (phdr.p_vaddr + phdr.p_memsz) > USER_STACK_BASE) {
            sprintln("[ELF] ERROR: Segment overlaps stack base");
            return -4;
        }

        uint64_t pte_flags = PTE_USER;
        if (phdr.p_flags & PF_W)    pte_flags |= PTE_WRITABLE;
        if (!(phdr.p_flags & PF_X)) pte_flags |= PTE_NX;

        if (!add_vma(filename, phdr.p_vaddr, phdr.p_memsz,
                     phdr.p_offset, phdr.p_filesz, pte_flags))
        {
            sprintln("[ELF] ERROR: Too many PT_LOAD segments");
            return -5;
        }
    }
    sprintln("[ELF] All PT_LOAD segments registered for demand paging.");

    sprintln("[ELF] Allocating user stack space...");

    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)((cr3 & ~0xFFFULL) + hhdm_offset);

    for (uint64_t i = 0; i < USER_STACK_PAGE_COUNT; i++)
    {
        uint64_t vaddr = USER_STACK_BASE + (i * PAGE_SIZE);
        uintptr_t page_phys = alloc_page();

        if (!page_phys) {
            sprintln("[ELF] ERROR: Stack allocation failed");
            return -7;
        }

        memset((void *)(page_phys + hhdm_offset), 0, PAGE_SIZE);

        if (!map_page(vaddr, page_phys, PTE_USER | PTE_WRITABLE | PTE_NX)) {
            free_page(page_phys);
            sprintln("[ELF] ERROR: Stack page mapping failed");
            return -7;
        }

        size_t pml4_i = (vaddr >> 39) & 0x1FF;
        size_t pdpt_i = (vaddr >> 30) & 0x1FF;
        size_t pd_i   = (vaddr >> 21) & 0x1FF;
        size_t pt_i   = (vaddr >> 12) & 0x1FF;

        pml4[pml4_i] |= (PTE_USER | PTE_WRITABLE);
        uint64_t *pdpt = (uint64_t *)((pml4[pml4_i] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
        pdpt[pdpt_i] |= (PTE_USER | PTE_WRITABLE);
        uint64_t *pd = (uint64_t *)((pdpt[pdpt_i] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
        pd[pd_i] |= (PTE_USER | PTE_WRITABLE);
        uint64_t *pt = (uint64_t *)((pd[pd_i] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
        pt[pt_i] |= (PTE_USER | PTE_WRITABLE);

        __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
    }
    sprintln("[ELF] User stack allocated and mapped successfully.");
    uint64_t stack_virt = USER_STACK_TOP;
    if (argc < 0) argc = 0;
    if (argc > 16) argc = 16; // Safety limit

    uint64_t user_argv_vaddrs[16];

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        stack_virt -= len;
        void *dest = get_hhdm_ptr(stack_virt);
        if (dest) {
            memcpy(dest, argv[i], len);
        }
        user_argv_vaddrs[i] = stack_virt;
    }

    stack_virt &= ~0xFULL;
    stack_virt -= sizeof(uint64_t);
    void *null_dest = get_hhdm_ptr(stack_virt);
    if (null_dest) {
        *(uint64_t *)null_dest = 0;
    }

    for (int i = argc - 1; i >= 0; i--) {
        stack_virt -= sizeof(uint64_t);
        void *ptr_dest = get_hhdm_ptr(stack_virt);
        if (ptr_dest) {
            *(uint64_t *)ptr_dest = user_argv_vaddrs[i];
        }
    }

    uint64_t argv_ptr_array_vaddr = stack_virt;

    stack_virt &= ~0xFULL;

    bootstrap_cpu_gs.kernel_stack = (uint64_t)(bootstrap_kernel_stack_space + KERNEL_STACK_SIZE);

    sprint("[ELF] Bootstrap Kernel Stack Top assigned at: ");
    uint64_to_str(bootstrap_cpu_gs.kernel_stack, buf);
    sprintln(buf);

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

    to_userland_ring3_args(ehdr.e_entry, stack_virt, (uint64_t)argc, argv_ptr_array_vaddr);
    sprintln("[ELF] returned");
    sti();
    return 0;
}