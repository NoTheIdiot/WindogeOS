#include <stdint.h>
#include <boot/limine.h>
#include <boot/kernel.h>
#include <core.h>

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE   (1ULL << 1)
#define PAGE_USER    (1ULL << 2)

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;

uint64_t pmm_alloc_block(void) {
    static uint64_t memmap_idx = 0;
    static uint64_t current_addr = 0;
    struct limine_memmap_response *memmap = memmap_request.response;

    while (memmap_idx < memmap->entry_count) {
        struct limine_memmap_entry *entry = memmap->entries[memmap_idx];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            if (current_addr == 0 || current_addr < entry->base) {
                current_addr = entry->base;
            }
            if (current_addr + 4096 <= entry->base + entry->length) {
                uint64_t frame = current_addr;
                current_addr += 4096;
                return frame;
            }
        }
        memmap_idx++;
        current_addr = 0;
    }
    return 0;
}

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_request.response->offset);
}

uint64_t* get_limine_boot_pml4(void) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r" (cr3));
    return (uint64_t *)phys_to_virt(cr3 & 0xFFFFFFFFF000ULL);
}

uint64_t create_user_pml4(void) {
    uint64_t pml4_phys = pmm_alloc_block();
    uint64_t *pml4_virt = (uint64_t *)phys_to_virt(pml4_phys);

    for (int i = 0; i < 512; i++) {
        pml4_virt[i] = 0;
    }

    uint64_t *boot_pml4 = get_limine_boot_pml4();
    
    // Deep copy higher-half (kernel space) entries safely
    for (int i = 256; i < 512; i++) {
        if (boot_pml4[i] & PAGE_PRESENT) {
            uint64_t src_pdpt_phys = boot_pml4[i] & 0xFFFFFFFFF000ULL;
            uint64_t *src_pdpt = (uint64_t *)phys_to_virt(src_pdpt_phys);
            
            uint64_t dst_pdpt_phys = pmm_alloc_block();
            uint64_t *dst_pdpt = (uint64_t *)phys_to_virt(dst_pdpt_phys);

            for (int j = 0; j < 512; j++) {
                dst_pdpt[j] = src_pdpt[j];
            }

            uint64_t flags = boot_pml4[i] & 0xFFF;
            pml4_virt[i] = dst_pdpt_phys | flags;
        }
    }

    return pml4_phys;
}

void map_user_page(uint64_t pml4_phys, uint64_t virtual_addr, uint64_t physical_addr) {
    uint64_t *pml4 = (uint64_t *)phys_to_virt(pml4_phys);

    uint64_t pml4_idx = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx  = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_idx    = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_idx    = (virtual_addr >> 12) & 0x1FF;

    uint64_t *pdpt, *pd, *pt;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        uint64_t pdpt_phys = pmm_alloc_block();
        pdpt = (uint64_t *)phys_to_virt(pdpt_phys);
        for (int i = 0; i < 512; i++) pdpt[i] = 0;
        pml4[pml4_idx] = pdpt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        pdpt = (uint64_t *)phys_to_virt(pml4[pml4_idx] & 0xFFFFFFFFF000ULL);
    }

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        uint64_t pd_phys = pmm_alloc_block();
        pd = (uint64_t *)phys_to_virt(pd_phys);
        for (int i = 0; i < 512; i++) pd[i] = 0;
        pdpt[pdpt_idx] = pd_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        pd = (uint64_t *)phys_to_virt(pdpt[pdpt_idx] & 0xFFFFFFFFF000ULL);
    }

    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        uint64_t pt_phys = pmm_alloc_block();
        pt = (uint64_t *)phys_to_virt(pt_phys);
        for (int i = 0; i < 512; i++) pt[i] = 0;
        pd[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        pt = (uint64_t *)phys_to_virt(pd[pd_idx] & 0xFFFFFFFFF000ULL);
    }

    pt[pt_idx] = (physical_addr & 0xFFFFFFFFF000ULL) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
}

void init_user_space(void) {
    uint64_t user_pml4 = create_user_pml4();

    uint64_t code_phys = pmm_alloc_block();
    uint64_t stack_phys = pmm_alloc_block();

    uint64_t user_virt_code = 0x400000;
    uint64_t user_virt_stack = 0x800000;

    map_user_page(user_pml4, user_virt_code, code_phys);
    map_user_page(user_pml4, user_virt_stack - 4096, stack_phys);

    unsigned char app_bytes[] = { 0xEB, 0xFE };
    unsigned char *dest = (unsigned char *)phys_to_virt(code_phys);
    for (int i = 0; i < (int)sizeof(app_bytes); i++) {
        dest[i] = app_bytes[i];
    }

    core_to_user(user_pml4, (void *)user_virt_code, (void *)user_virt_stack);
}