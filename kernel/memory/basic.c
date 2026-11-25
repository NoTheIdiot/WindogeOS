#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <limine.h>
#include <io.h>
#include <bool.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

typedef struct free {
    struct free *next;
} free;

free *free_head = NULL;
uintptr_t hhdm_offset = 0;
uintptr_t kernel_pml4_phys = 0;

static inline uintptr_t get_cr3(void) {
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & PAGE_ADDRESS_MASK;
}

static inline void set_cr3(uintptr_t pml4_phys) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

void free_page(uintptr_t phys_addr) {
    if (phys_addr == 0) return;

    uintptr_t aligned_phys = phys_addr & PAGE_ADDRESS_MASK;
    free *page = (free *)(aligned_phys + hhdm_offset);

    page->next = free_head;
    free_head = page;
}

static void free_user_table_level(uintptr_t table_phys, int level) {
    if (!table_phys) return;

    uint64_t *table = (uint64_t *)((table_phys & PAGE_ADDRESS_MASK) + hhdm_offset);

    for (size_t i = 0; i < 512; i++) {
        uint64_t entry = table[i];
        if (!(entry & PAGE_PRESENT)) {
            continue;
        }

        uintptr_t child_phys = entry & PAGE_ADDRESS_MASK;

        if (level == 1) {
            if (child_phys != 0) {
                free_page(child_phys);
            }
        } else {
            if (!(entry & PAGE_HUGE) && child_phys != 0) {
                free_user_table_level(child_phys, level - 1);
            }
        }

        table[i] = 0;
    }

    free_page(table_phys);
}

uintptr_t create_pml4(void)
{
    uintptr_t phys = alloc_page();

    if (!phys)
        return 0;

    uint64_t *new_pml4 =
        (uint64_t *)(phys + hhdm_offset);

    uint64_t *kernel_pml4 =
        (uint64_t *)(kernel_pml4_phys + hhdm_offset);

    zero_page_4k(new_pml4);

    for (size_t i = 256; i < 512; i++)
    {
        new_pml4[i] = kernel_pml4[i];
    }

    return phys;
}

static uint64_t *get_next_level_for_pml4(
    uint64_t *table,
    size_t index,
    bool allocate,
    uint64_t flags)
{
    if (table[index] & PAGE_PRESENT)
    {
        if (table[index] & PAGE_HUGE)
            return NULL;

        uintptr_t next_phys =
            table[index] & PAGE_ADDRESS_MASK;

        return (uint64_t *)
            (next_phys + hhdm_offset);
    }

    if (!allocate)
        return NULL;

    uintptr_t phys = alloc_page();

    if (!phys)
        return NULL;

    uint64_t *next =
        (uint64_t *)(phys + hhdm_offset);

    zero_page_4k(next);

    table[index] =
        phys |
        PAGE_PRESENT |
        PAGE_WRITABLE |
        (flags & PAGE_USER);

    return next;
}

bool map_page_pml4(
    uintptr_t pml4_phys,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    uint64_t flags)
{
    uint64_t *pml4 =
        (uint64_t *)(pml4_phys + hhdm_offset);

    size_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    size_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    size_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    size_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    uint64_t *pdpt =
        get_next_level_for_pml4(
            pml4,
            pml4_idx,
            true,
            flags);

    if (!pdpt)
        return false;

    uint64_t *pd =
        get_next_level_for_pml4(
            pdpt,
            pdpt_idx,
            true,
            flags);

    if (!pd)
        return false;

    uint64_t *pt =
        get_next_level_for_pml4(
            pd,
            pd_idx,
            true,
            flags);

    if (!pt)
        return false;

    pt[pt_idx] =
        (phys_addr & PAGE_ADDRESS_MASK) |
        flags |
        PAGE_PRESENT;

    return true;
}

uintptr_t unmap_page_pml4(
    uintptr_t pml4_phys,
    uintptr_t virt_addr)
{
    uint64_t *pml4 =
        (uint64_t *)(pml4_phys + hhdm_offset);

    size_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    size_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    size_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    size_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    uint64_t *pdpt =
        get_next_level_for_pml4(
            pml4,
            pml4_idx,
            false,
            0);

    if (!pdpt)
        return 0;

    uint64_t *pd =
        get_next_level_for_pml4(
            pdpt,
            pdpt_idx,
            false,
            0);

    if (!pd)
        return 0;

    uint64_t *pt =
        get_next_level_for_pml4(
            pd,
            pd_idx,
            false,
            0);

    if (!pt)
        return 0;

    if (!(pt[pt_idx] & PAGE_PRESENT))
        return 0;

    uintptr_t phys =
        pt[pt_idx] & PAGE_ADDRESS_MASK;

    pt[pt_idx] = 0;

    return phys;
}

void destroy_pml4(uintptr_t pml4_phys)
{
    if (!pml4_phys)
        return;

    uint64_t *pml4 =
        (uint64_t *)(pml4_phys + hhdm_offset);

    for (size_t i = 0; i < 256; i++)
    {
        if (!(pml4[i] & PAGE_PRESENT))
            continue;

        uintptr_t pdpt =
            pml4[i] & PAGE_ADDRESS_MASK;

        free_user_table_level(pdpt, 3);
    }

    free_page(pml4_phys);
}


void cleanup_pages(void) {
    if (!kernel_pml4_phys || !hhdm_offset) return;

    uint64_t *pml4 = (uint64_t *)(kernel_pml4_phys + hhdm_offset);

    for (size_t i = 0; i < 256; i++) {
        uint64_t entry = pml4[i];

        if (entry & PAGE_PRESENT) {
            uintptr_t pdpt_phys = entry & PAGE_ADDRESS_MASK;
            if (pdpt_phys != 0) {
                free_user_table_level(pdpt_phys, 3);
            }
            pml4[i] = 0;
        }
    }

    set_cr3(kernel_pml4_phys);
}

void zero_page_4k(void *virt_addr) {
    uint64_t *p = (uint64_t *)virt_addr;
    for (size_t i = 0; i < 512; i++) {
        p[i] = 0;
    }
}

static uint64_t *get_next_level(uint64_t *current_table, size_t index, bool allocate, uint64_t flags) {
    if (current_table[index] & PAGE_PRESENT) {
        if (current_table[index] & PAGE_HUGE) {
            return NULL;
        }

        if ((flags & PAGE_USER) && !(current_table[index] & PAGE_USER)) {
            current_table[index] |= PAGE_USER;
        }
        if ((flags & PAGE_WRITABLE) && !(current_table[index] & PAGE_WRITABLE)) {
            current_table[index] |= PAGE_WRITABLE;
        }

        uintptr_t next_phys = current_table[index] & PAGE_ADDRESS_MASK;
        return (uint64_t *)(next_phys + hhdm_offset);
    }

    if (!allocate) {
        return NULL;
    }

    uintptr_t new_phys = alloc_page();
    if (!new_phys) {
        return NULL;
    }

    uint64_t *new_table = (uint64_t *)((new_phys & PAGE_ADDRESS_MASK) + hhdm_offset);
    zero_page_4k(new_table);

    uint64_t intermediate_flags = PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    current_table[index] = (new_phys & PAGE_ADDRESS_MASK) | intermediate_flags;

    return new_table;
}

bool map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t flags) {
    if (!kernel_pml4_phys || !hhdm_offset) return false;

    size_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    size_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    size_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    size_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)(kernel_pml4_phys + hhdm_offset);

    uint64_t *pdpt = get_next_level(pml4, pml4_idx, true, flags);
    if (!pdpt) return false;

    uint64_t *pd = get_next_level(pdpt, pdpt_idx, true, flags);
    if (!pd) return false;

    uint64_t *pt = get_next_level(pd, pd_idx, true, flags);
    if (!pt) return false;

    pt[pt_idx] = (phys_addr & PAGE_ADDRESS_MASK) | flags | PAGE_PRESENT;
    invlpg(virt_addr);

    return true;
}

uintptr_t unmap_page(uintptr_t virt_addr) {
    if (!kernel_pml4_phys || !hhdm_offset) return 0;

    size_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    size_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    size_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    size_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)(kernel_pml4_phys + hhdm_offset);

    uint64_t *pdpt = get_next_level(pml4, pml4_idx, false, 0);
    if (!pdpt) return 0;

    uint64_t *pd = get_next_level(pdpt, pdpt_idx, false, 0);
    if (!pd) return 0;

    uint64_t *pt = get_next_level(pd, pd_idx, false, 0);
    if (!pt) return 0;

    if (!(pt[pt_idx] & PAGE_PRESENT)) {
        return 0;
    }

    uintptr_t phys_addr = pt[pt_idx] & PAGE_ADDRESS_MASK;
    pt[pt_idx] = 0;

    invlpg(virt_addr);

    return phys_addr;
}

void init_vmm(void) {
    kernel_pml4_phys = alloc_page();
    if (!kernel_pml4_phys) {
        halt();
    }

    uint64_t *new_pml4 = (uint64_t *)((kernel_pml4_phys & PAGE_ADDRESS_MASK) + hhdm_offset);
    uint64_t *old_pml4 = (uint64_t *)((get_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);

    zero_page_4k(new_pml4);

    for (size_t i = 256; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }

    set_cr3(kernel_pml4_phys);
}

void init_memory(void) {
    struct limine_memmap_response *memmap = memmap_request.response;
    struct limine_hhdm_response *hhdm = hhdm_request.response;

    if (!memmap || !hhdm) {
        halt();
    }

    hhdm_offset = hhdm->offset;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start = ALIGN_UP(entry->base, PAGE_SIZE);
            uint64_t end   = ALIGN_DOWN(entry->base + entry->length, PAGE_SIZE);
            for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
                free_page(addr);
            }
        }
    }

    init_vmm();
}