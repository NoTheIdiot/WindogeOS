#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <system.h>
#include <process.h>

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

extern uint64_t allocate_pid(void);

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

static void clear_vmas(process *proc)
{
    proc->vma_count = 0;

    for (size_t i = 0; i < MAX_VMAS; i++)
    {
        proc->vmas[i].active = false;
    }
}

static bool add_vma(
    process *proc,
    const char *filename,
    uint64_t vaddr,
    uint64_t memsz,
    uint64_t offset,
    uint64_t filesz,
    uint64_t flags)
{
    if (proc->vma_count >= MAX_VMAS)
        return false;

    vma_region_t *vma =
        &proc->vmas[proc->vma_count++];

    memset(vma, 0, sizeof(*vma));

    vma->active = true;

    size_t j = 0;

    while (filename[j] && j < sizeof(vma->filename) - 1)
    {
        vma->filename[j] = filename[j];
        j++;
    }

    vma->filename[j] = '\0';

    vma->p_vaddr    = vaddr;
    vma->p_memsz    = memsz;
    vma->p_offset   = offset;
    vma->p_filesz   = filesz;
    vma->pte_flags  = flags;

    return true;
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

bool try_demand_page(
    uint64_t fault_addr,
    uint64_t err_code)
{
    if (!current_proc)
        return false;

    vma_region_t *vma = NULL;

    for (size_t i = 0;
         i < current_proc->vma_count;
         i++)
    {
        vma_region_t *candidate =
            &current_proc->vmas[i];

        if (!candidate->active)
            continue;

        if (fault_addr >= align_down_page(candidate->p_vaddr) &&
            fault_addr < align_up_page(
                candidate->p_vaddr +
                candidate->p_memsz))
        {
            vma = candidate;
            break;
        }
    }

    if (!vma)
        return false;

    if ((err_code & (1ULL << 1)) &&
        !(vma->pte_flags & PTE_WRITABLE))
    {
        return false;
    }

    uint64_t page_vaddr =
        align_down_page(fault_addr);

    uintptr_t page_phys =
        alloc_page();

    if (!page_phys)
        return false;

    memset(
        (void *)(page_phys + hhdm_offset),
        0,
        PAGE_SIZE);

    uint64_t file_start =
        vma->p_vaddr;

    uint64_t file_end =
        vma->p_vaddr +
        vma->p_filesz;

    uint64_t page_end =
        page_vaddr +
        PAGE_SIZE;

    uint64_t copy_start =
        (page_vaddr > file_start)
            ? page_vaddr
            : file_start;

    uint64_t copy_end =
        (page_end < file_end)
            ? page_end
            : file_end;

    if (copy_start < copy_end)
    {
        uint64_t file_offset =
            vma->p_offset +
            (copy_start - file_start);

        uint64_t copy_size =
            copy_end - copy_start;

        uint8_t *dest =
            (uint8_t *)
            (page_phys +
             hhdm_offset +
             (copy_start - page_vaddr));

        __asm__ volatile("sti");

        int bytes_read =
            fs_read_raw_at(
                vma->filename,
                dest,
                file_offset,
                copy_size);

        __asm__ volatile("cli");

        if (bytes_read != (int)copy_size)
        {
            free_page(page_phys);
            return false;
        }
    }

    if (!map_page(
            page_vaddr,
            page_phys,
            vma->pte_flags))
    {
        free_page(page_phys);
        return false;
    }

    __asm__ volatile(
        "invlpg (%0)"
        :
        : "r"(page_vaddr)
        : "memory");

    return true;
}

process *exec(char *filename, int argc, char **argv) {
    elf64_ehdr ehdr;

    if (!filename)
        return NULL;

    if (fs_read_raw_at(
            filename,
            (uint8_t *)&ehdr,
            0,
            sizeof(ehdr))
        != (int)sizeof(ehdr))
    {
        return NULL;
    }

    if (ehdr.e_ident[0] != 0x7F ||
        ehdr.e_ident[1] != 'E'  ||
        ehdr.e_ident[2] != 'L'  ||
        ehdr.e_ident[3] != 'F')
    {
        return NULL;
    }

    if (ehdr.e_ident[4] != ELF_CLASS_64)
        return NULL;

    if (ehdr.e_ident[5] != ELF_DATA_LSB)
        return NULL;

    if (ehdr.e_version != ELF_VERSION_CURRENT)
        return NULL;

    if (ehdr.e_type != ELF_TYPE_EXEC)
        return NULL;

    if (ehdr.e_machine != ELF_MACHINE_X86_64)
        return NULL;

    if (ehdr.e_phentsize != sizeof(elf64_phdr))
        return NULL;

    if (ehdr.e_phnum == 0)
        return NULL;

    process *proc =
        (process *)kmalloc(sizeof(process));

    if (!proc)
        return NULL;

    memset(proc, 0, sizeof(process));

    proc->pid   = allocate_pid();
    proc->state = PROC_WAITING;

    clear_vmas(proc);

    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        elf64_phdr phdr;

        uint64_t phdr_offset =
            ehdr.e_phoff +
            ((uint64_t)i * ehdr.e_phentsize);

        if (fs_read_raw_at(
                filename,
                (uint8_t *)&phdr,
                phdr_offset,
                sizeof(phdr))
            != (int)sizeof(phdr))
        {
            kfree(proc);
            return NULL;
        }

        if (phdr.p_type != PT_LOAD)
            continue;

        if (phdr.p_memsz == 0)
            continue;

        if (phdr.p_memsz < phdr.p_filesz)
        {
            kfree(proc);
            return NULL;
        }

        if (phdr.p_memsz > UINT64_MAX - phdr.p_vaddr)
        {
            kfree(proc);
            return NULL;
        }

        if (phdr.p_vaddr >= USER_STACK_BASE)
        {
            kfree(proc);
            return NULL;
        }

        if ((phdr.p_vaddr + phdr.p_memsz) > USER_STACK_BASE)
        {
            kfree(proc);
            return NULL;
        }

        uint64_t flags = PTE_USER;

        if (phdr.p_flags & PF_W)
            flags |= PTE_WRITABLE;

        if (!(phdr.p_flags & PF_X))
            flags |= PTE_NX;

        if (!add_vma(
                proc,
                filename,
                phdr.p_vaddr,
                phdr.p_memsz,
                phdr.p_offset,
                phdr.p_filesz,
                flags))
        {
            kfree(proc);
            return NULL;
        }

        if (proc->region_count >= MAX_PROC_REGIONS)
        {
            kfree(proc);
            return NULL;
        }

        vm_region_t *region =
            &proc->regions[proc->region_count++];

        region->start =
            align_down_page(phdr.p_vaddr);

        region->end =
            align_up_page(
                phdr.p_vaddr +
                phdr.p_memsz);

        region->flags = flags;
    }

    if (argc < 0)
        argc = 0;

    if (argc > 64)
        argc = 64;

    if (!proc_create_stack(proc))
    {
        kfree(proc);
        return NULL;
    }

    uint64_t argv_ptr =
        proc_build_argv(
            proc,
            argc,
            argv);

    proc->entry_point =
        ehdr.e_entry;

    proc->argc =
        argc;

    proc->argv =
        argv_ptr;

    proc->saved_rsp =
        proc->stack_top;

    proc->kernel_stack_base =
        (uint64_t)kmalloc(
            KERNEL_STACK_SIZE);

    if (!proc->kernel_stack_base)
    {
        kfree(proc);
        return NULL;
    }

    memset(
        (void *)proc->kernel_stack_base,
        0,
        KERNEL_STACK_SIZE);

    proc->saved_kernel_rsp =
        proc->kernel_stack_base +
        KERNEL_STACK_SIZE;

    return proc;
}