#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <boot/limine.h>
#include <basicutil.h>
#include <string.h>
#include <dogeio.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

#define PAGE_USER_FLAGS (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request   hhdm_request;

uint64_t pmm_alloc_zeroed_page(void) {
    static uint64_t alloc_index = 0;
    static uint64_t alloc_offset = 0;

    if (!memmap_request.response || !hhdm_request.response) {
        dogeio_text_println("[Error] Bootloader requests not found, try rebooting.");
        cli();
        halt();
    }

    struct limine_memmap_response *memmap = memmap_request.response;
    uint64_t hhdm_offset = hhdm_request.response->offset;

    for (; alloc_index < memmap->entry_count; alloc_index++) {
        struct limine_memmap_entry *entry = memmap->entries[alloc_index];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            alloc_offset = 0;
            continue;
        }

        if (alloc_offset + 4096 <= entry->length) {
            uint64_t phys_addr = entry->base + alloc_offset;
            alloc_offset += 4096;

            memset((void *)(phys_addr + hhdm_offset), 0, 4096);
            return phys_addr;
        }
        
        alloc_offset = 0;
    }

    dogeio_text_println("[error] out of dang ram");
    cli();
    halt();
    return 0;
}

static inline uint64_t* get_or_alloc_table(uint64_t* table, size_t index, uint64_t hhdm_offset) {
    if (!(table[index] & PAGE_PRESENT)) {
        table[index] = pmm_alloc_zeroed_page() | PAGE_USER_FLAGS;
    } else {
        table[index] |= PAGE_USER_FLAGS;
    }
    return (uint64_t *)((table[index] & ~0xFFFULL) + hhdm_offset);
}

void map_user_page(uint64_t virt_addr, uint64_t phys_addr) {
    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *pml4 = (uint64_t *)((read_cr3() & ~0xFFFULL) + hhdm_offset);

    uint64_t *pdpt = get_or_alloc_table(pml4, (virt_addr >> 39) & 0x1FF, hhdm_offset);
    uint64_t *pd   = get_or_alloc_table(pdpt, (virt_addr >> 30) & 0x1FF, hhdm_offset);
    uint64_t *pt   = get_or_alloc_table(pd,   (virt_addr >> 21) & 0x1FF, hhdm_offset);

    pt[(virt_addr >> 12) & 0x1FF] = (phys_addr & ~0xFFFULL) | PAGE_USER_FLAGS;
    
    invlpg(virt_addr);
}

void setup_ring3_memory(uint64_t user_code_virt, uint64_t user_stack_virt, const uint8_t *user_code, size_t code_size) {
    uint64_t hhdm_offset = hhdm_request.response->offset;

    for (size_t offset = 0; offset < code_size; offset += 4096) {
        uint64_t code_phys = pmm_alloc_zeroed_page();
        size_t bytes_to_copy = (code_size - offset > 4096) ? 4096 : (code_size - offset);
        
        memcpy((void *)(code_phys + hhdm_offset), user_code + offset, bytes_to_copy);
        map_user_page(user_code_virt + offset, code_phys);
    }

    uint64_t stack_phys = pmm_alloc_zeroed_page();
    map_user_page(user_stack_virt, stack_phys);
}