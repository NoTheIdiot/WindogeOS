#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <boot/limine.h>
#include <basicutil.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

#define PAGE_USER_FLAGS (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)

// get limine memory requests
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request   hhdm_request;

static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline void invlpg(uint64_t vaddr) {
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
}

static inline pmm_alloc_zeored_page(void) {
    if (!memmap_request.response || !hhdm_request.response) {
        while (1) __asm__ volatile("cli; hlt");
    }

    struct limine_memmap_response *memmap = memmap_request.response;
    uint64_t hhdm_offset = hhdm_request.response->offset;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= 4096) {
            uint64_t phys_addr = entry->base;

            entry->base += 4096;
            entry->length -= 4096;

            void *virt_ptr = (void *)(phys_addr + hhdm_offset);
            memset(virt_ptr, 0, 4096);

            return phys_addr;
        }
    }

    while (1) {
        cli();
        halt();
    }
}