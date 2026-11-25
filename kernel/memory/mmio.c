#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <io.h>

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;

typedef struct free {
    struct free *next;
} free;

extern free *free_head;
extern uintptr_t hhdm_offset;

#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_PWT      (1ULL << 3)
#define PTE_PCD      (1ULL << 4)
#define PTE_HUGE     (1ULL << 7)
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

static void zero_4k(uint8_t *virt) {
    for (size_t i = 0; i < 4096; i++) {
        virt[i] = 0;
    }
}

uint64_t alloc_page(void) {
    uint64_t hhdm = hhdm_offset ? hhdm_offset
                               : (hhdm_request.response ? hhdm_request.response->offset : 0);

    /* 1. Reuse freed physical pages first so RAM doesn't exhaust on -m 5M */
    if (free_head && hhdm) {
        free *node = free_head;
        free_head = node->next;
        node->next = NULL;

        uint64_t phys = ((uint64_t)(uintptr_t)node - hhdm) & PTE_ADDR_MASK;
        zero_4k((uint8_t *)(uintptr_t)(phys + hhdm));
        return phys;
    }

    /* 2. Fallback bump allocator across usable Limine memory regions */
    static size_t entry_idx = 0;
    static uint64_t offset = 0;

    struct limine_memmap_response *mmap = memmap_request.response;
    if (!mmap || !hhdm) return 0;

    while (entry_idx < mmap->entry_count) {
        struct limine_memmap_entry *e = mmap->entries[entry_idx];
        if (e->type == LIMINE_MEMMAP_USABLE && offset + 4096 <= e->length) {
            uint64_t phys = e->base + offset;
            offset += 4096;
            zero_4k((uint8_t *)(uintptr_t)(phys + hhdm));
            return phys;
        }
        entry_idx++;
        offset = 0;
    }
    return 0;
}

static uint64_t *get_or_alloc_table(uint64_t *table, size_t idx) {
    uint64_t hhdm = hhdm_offset ? hhdm_offset : hhdm_request.response->offset;
    if (!(table[idx] & PTE_PRESENT)) {
        uint64_t new_phys = alloc_page();
        if (!new_phys) return NULL;
        table[idx] = new_phys | PTE_PRESENT | PTE_WRITABLE;
    }
    return (uint64_t *)((table[idx] & PTE_ADDR_MASK) + hhdm);
}

void *map_kernel_mmio(uint64_t phys_addr, size_t size) {
    uint64_t hhdm = hhdm_offset ? hhdm_offset : hhdm_request.response->offset;
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    uint64_t *pml4 = (uint64_t *)((cr3 & PTE_ADDR_MASK) + hhdm);
    uint64_t start = phys_addr & ~0xFFFULL;
    uint64_t end   = (phys_addr + size + 0xFFFULL) & ~0xFFFULL;

    for (uint64_t p = start; p < end; p += 4096) {
        uint64_t v = p + hhdm;
        size_t i4 = (v >> 39) & 0x1FF;
        size_t i3 = (v >> 30) & 0x1FF;
        size_t i2 = (v >> 21) & 0x1FF;
        size_t i1 = (v >> 12) & 0x1FF;

        uint64_t *pdpt = get_or_alloc_table(pml4, i4);
        if (!pdpt) return NULL;
        uint64_t *pd = get_or_alloc_table(pdpt, i3);
        if (!pd) return NULL;
        uint64_t *pt = get_or_alloc_table(pd, i2);
        if (!pt) return NULL;

        pt[i1] = p | PTE_PRESENT | PTE_WRITABLE | PTE_PWT | PTE_PCD;
        __asm__ volatile ("invlpg (%0)" :: "r"(v) : "memory");
    }

    return (void *)(phys_addr + hhdm);
}