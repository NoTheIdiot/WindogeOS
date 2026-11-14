#include <system.h>
#include <stdint.h>
#include <limine.h>
#include <string.h>
#include <dogeio.h>

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;

static uint64_t free_page_head;

#define PAGE_MASK       0xFFFULL
#define FREE_PAGE_MAGIC 0xF00DF00DDEADFACEULL

static void pmm_free_page(uint64_t phys_addr, uint64_t hhdm_offset) {
    if (phys_addr == 0 || (phys_addr & PAGE_MASK) != 0) {
        __asm__ volatile ("cli; hlt");
        for (;;);
    }

    static volatile int lock = 0;
    uint64_t rflags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(rflags) :: "memory");
    while (__sync_lock_test_and_set(&lock, 1)) {
        __builtin_ia32_pause();
    }

    uint64_t *page = (uint64_t *)(phys_addr + hhdm_offset);

    if (page[1] == FREE_PAGE_MAGIC) {
        __asm__ volatile ("cli; hlt");
        for (;;);
    }

    page[0] = free_page_head;
    page[1] = FREE_PAGE_MAGIC;
    free_page_head = phys_addr;

    __sync_lock_release(&lock);
    if (rflags & (1ULL << 9)) {
        __asm__ volatile ("sti" ::: "memory");
    }
}


static void free_user_table(uint64_t phys_addr, unsigned int level,
                            uint64_t hhdm_offset) {
    uint64_t *table = (uint64_t *)(phys_addr + hhdm_offset);

    for (size_t i = 0; i < 512; i++) {
        uint64_t entry = table[i];
        if ((entry & PAGE_USER) == 0) {
            continue;
        }

        uint64_t child_phys = level == 1 ?
                            entry & PAGE_ADDRESS_MASK : entry & ~0xFFFULL;
        if (level == 1) {
            if (child_phys != 0) {
                pmm_free_page(child_phys, hhdm_offset);
            }
        } else if ((entry & PAGE_PRESENT) != 0) {
            free_user_table(child_phys, level - 1U, hhdm_offset);
        }
    }

    pmm_free_page(phys_addr, hhdm_offset);
}

uintptr_t map_kernel_mmio_page(uintptr_t virt_addr, uint64_t phys_addr) {
    if (hhdm_request.response == NULL ||
        (virt_addr & 0xFFFULL) != 0 ||
        (phys_addr & 0xFFFULL) != 0) {
        return 0;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *table = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) +
                                    hhdm_offset);
    const size_t indices[] = {
        (size_t)((virt_addr >> 39) & 0x1FF),
        (size_t)((virt_addr >> 30) & 0x1FF),
        (size_t)((virt_addr >> 21) & 0x1FF),
        (size_t)((virt_addr >> 12) & 0x1FF)
    };

    for (size_t level = 0; level < 3; level++) {
        uint64_t entry = table[indices[level]];
        if ((entry & PAGE_PRESENT) != 0) {
            if ((entry & PAGE_LARGE) != 0) {
                return 0;
            }
            table = (uint64_t *)((entry & PAGE_ADDRESS_MASK) + hhdm_offset);
            continue;
        }

        uint64_t next_table_phys = pmm_alloc_zeroed_page();
        if (next_table_phys == 0) {
            return 0;
        }
        table[indices[level]] = next_table_phys |
                              PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        table = (uint64_t *)(next_table_phys + hhdm_offset);
    }

    uint64_t *entry = &table[indices[3]];
    if ((*entry & PAGE_PRESENT) != 0 &&
        (*entry & PAGE_ADDRESS_MASK) != (phys_addr & PAGE_ADDRESS_MASK)) {
        return 0;
    }

    *entry = (phys_addr & PAGE_ADDRESS_MASK) | PAGE_PRESENT | PAGE_WRITABLE |
             PAGE_USER | PAGE_WRITE_THROUGH | PAGE_CACHE_DISABLE;
    if ((rdmsr(MSR_IA32_EFER) & EFER_NXE) != 0) {
        *entry |= PAGE_NO_EXECUTE;
    }
    invlpg(virt_addr);
    return virt_addr;
}