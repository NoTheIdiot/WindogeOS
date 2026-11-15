#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>
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
    return cr3 & PTE_ADDR_MASK;
}

static inline void set_cr3(uintptr_t pml4_phys) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

void free_page(uintptr_t phys_addr) {
    // do not clear literally the BIOS (or UEFI)
    if (phys_addr == 0) return;

    // convert it into actual usable address
    free *page = (free*)(phys_addr + hhdm_offset);
    
    // make the computer just ignore it and mark as free
    page->next = free_head;
    free_head = page;
}

uintptr_t alloc_page(void) {
    // uh, you are out of memory....
    if (!free_head) return 0;

    free *page = free_head;
    free_head = page->next;

    uint8_t* bytes = (uint8_t*)page;
    for (size_t i = 0; i < PAGE_SIZE; i++) {
        bytes[i] = 0;
    }

    return (uintptr_t)page - hhdm_offset;
}

static void free_user_table_level(uintptr_t table_phys, int level) {
    if (!table_phys) return;

    uint64_t *table = (uint64_t *)(table_phys + hhdm_offset);

    for (size_t i = 0; i < 512; i++) {
        uint64_t entry = table[i];

        if (!(entry & PTE_PRESENT) || !(entry & PTE_USER)) {
            continue;
        }

        uintptr_t child_phys = entry & PTE_ADDR_MASK;

        if (level == 1) {
            if (child_phys != 0) {
                free_page(child_phys);
            }
        } else {
            if (!(entry & PTE_HUGE) && child_phys != 0) {
                free_user_table_level(child_phys, level - 1);
            }
        }

        table[i] = 0;
    }

    free_page(table_phys);
}

void cleanup_pages(void) {
    if (!kernel_pml4_phys || !hhdm_offset) return;

    uint64_t *pml4 = (uint64_t *)(kernel_pml4_phys + hhdm_offset);

    for (size_t i = 0; i < 256; i++) {
        uint64_t entry = pml4[i];

        if ((entry & PTE_PRESENT) && (entry & PTE_USER)) {
            uintptr_t pdpt_phys = entry & PTE_ADDR_MASK;
            if (pdpt_phys != 0) {
                free_user_table_level(pdpt_phys, 3);
            }
            pml4[i] = 0;
        }
    }

    set_cr3(kernel_pml4_phys);
}


// go down a level
static uint64_t* get_next_level(uint64_t *current_table, size_t index, bool allocate, uint64_t flags) {
    if (current_table[index] & PTE_PRESENT) {
        // HEY NO BIG PAGES >:(
        if (current_table[index] & PTE_HUGE) {
            return NULL;
        }
        uintptr_t next_phys = current_table[index] & PTE_ADDR_MASK;
        return (uint64_t *)(next_phys + hhdm_offset);
    }

    if (!allocate) {
        return NULL;
    }

    uintptr_t new_phys = alloc_page();
    if (!new_phys) {
        return NULL; // sam altman got you again
    }

    // hey, this page can be used anywhere
    uint64_t intermediate_flags = PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    current_table[index] = new_phys | intermediate_flags;

    return (uint64_t *)(new_phys + hhdm_offset);
}

// map a 4KB page in memory to tell the cpu
// that the ram there, actually exists and it's not in (void), get it?
// my jokes suck....
bool map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t flags) {
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

    pt[pt_idx] = (phys_addr & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    invlpg(virt_addr);

    return true;
}

// mnmap a 4 KiB virtual address and return its old physical address (or 0 if unmapped)
// so basically, telling the CPU that piece of memory, probably doesn't exist.
uintptr_t unmap_page(uintptr_t virt_addr) {
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

    if (!(pt[pt_idx] & PTE_PRESENT)) {
        return 0;
    }

    uintptr_t phys_addr = pt[pt_idx] & PTE_ADDR_MASK;
    pt[pt_idx] = 0;

    invlpg(virt_addr);

    return phys_addr;
}

void init_vmm(void) {
    kernel_pml4_phys = alloc_page();
    if (!kernel_pml4_phys) {
        halt();
    }

    uint64_t *new_pml4 = (uint64_t *)(kernel_pml4_phys + hhdm_offset);
    uint64_t *old_pml4 = (uint64_t *)(get_cr3() + hhdm_offset);

    // copy higher-half entries (HHDM and kernel mapping)
    for (size_t i = 256; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }

    // switch to our own page table
    set_cr3(kernel_pml4_phys);
}

void init_memory(void) {
    struct limine_memmap_response *memmap = memmap_request.response;
    struct limine_hhdm_response *hhdm = hhdm_request.response;

    if (!memmap || !hhdm) {
        // dude uhh nothinig is here
        halt();
    }

    hhdm_offset = hhdm->offset;

    // look for memmap entries
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        // skip reserved spots so i dont just destroy the OS memory
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t offset = 0; offset < entry->length; offset += PAGE_SIZE) {
                free_page(entry->base + offset);
            }
        }
    }

    // switch from Limine's PML4 to WINDOGE pml4
    init_vmm();
}
