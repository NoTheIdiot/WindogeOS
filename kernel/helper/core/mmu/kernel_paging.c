#include <system.h>
#include <stdint.h>
extern uintptr_t hhdm_offset;

#define PTE_PWT (1ULL << 3)
#define PTE_PCD (1ULL << 4)

// map a mmio page (why didnt they do this automatically?)
uintptr_t map_kernel_mmio_page(uintptr_t virt_addr, uintptr_t phys_addr) {
    if ((virt_addr & (PAGE_SIZE - 1)) != 0 ||
        (phys_addr & (PAGE_SIZE - 1)) != 0) {
        return 0;
    }

    uint64_t *table = (uint64_t *)(read_cr3() + hhdm_offset);
    const size_t indices[4] = {
        (size_t)((virt_addr >> 39) & 0x1FF),
        (size_t)((virt_addr >> 30) & 0x1FF),
        (size_t)((virt_addr >> 21) & 0x1FF),
        (size_t)((virt_addr >> 12) & 0x1FF)
    };

    for (size_t level = 0; level < 3; level++) {
        uint64_t entry = table[indices[level]];
        if (entry & PTE_PRESENT) {
            if (entry & PTE_HUGE) {
                return 0;
            }
            table = (uint64_t *)((entry & PTE_ADDR_MASK) + hhdm_offset);
            continue;
        }

        uintptr_t next_table_phys = alloc_page();
        if (next_table_phys == 0) {
            return 0;
        }

        table[indices[level]] = next_table_phys | PTE_PRESENT | PTE_WRITABLE;
        table = (uint64_t *)(next_table_phys + hhdm_offset);
    }

    uint64_t *entry = &table[indices[3]];
    if ((*entry & PTE_PRESENT) &&
        (*entry & PTE_ADDR_MASK) != (phys_addr & PTE_ADDR_MASK)) {
        return 0;
    }

    *entry = (phys_addr & PTE_ADDR_MASK) |
             PTE_PRESENT | PTE_WRITABLE | PTE_PWT | PTE_PCD | PTE_NX;

    invlpg(virt_addr);
    return virt_addr;
}

uintptr_t map_kernel_mmio_region(uintptr_t virt_base, uintptr_t phys_base, size_t size) {
    uintptr_t phys_aligned = phys_base & ~(uintptr_t)(PAGE_SIZE - 1);
    size_t offset = phys_base - phys_aligned;
    size_t total_bytes = size + offset;

    for (size_t pos = 0; pos < total_bytes; pos += PAGE_SIZE) {
        if (!map_kernel_mmio_page(virt_base + pos, phys_aligned + pos)) {
            return 0;
        }
    }

    return virt_base + offset;
}
