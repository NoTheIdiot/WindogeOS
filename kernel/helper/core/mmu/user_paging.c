#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <string.h>
#include <dogeio.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request   hhdm_request;

static uint64_t free_page_head = 0;

bool user_page_mapped(uint64_t virt_addr);
bool unmap_user_page(uint64_t virt_addr);

void init_vmm(void) {
    println("\033[36m[VMM]\033[0m     Initializing Virtual Memory Manager...");
    
    uint64_t efer = rdmsr(MSR_IA32_EFER);
    if ((efer & EFER_NXE) == 0) {
        wrmsr(MSR_IA32_EFER, efer | EFER_NXE);
    }
    
    free_page_head = 0;
    println("\033[32m[VMM]\033[0m     Virtual Memory Manager initialized successfully.");
}

void cleanup_program_pages(const program_memory_t *program) {
    if (program == NULL) {
        return;
    }

    for (uint64_t region = 0; region < program->region_count; region++) {
        uint64_t start = program->regions[region].start;
        uint64_t end = program->regions[region].end;

        for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
            if (user_page_mapped(addr)) {
                unmap_user_page(addr);
            }
        }
    }

    for (uint64_t i = 0; i < program->stack_pages; i++) {
        uint64_t addr = program->stack_base + (i * PAGE_SIZE);

        if (user_page_mapped(addr)) {
            unmap_user_page(addr);
        }
    }
}

uint64_t pmm_alloc_zeroed_page(void) {
    static uint64_t alloc_index = 0;
    static uint64_t alloc_offset = 0;

    if (!memmap_request.response || !hhdm_request.response) {
        println("\033[31m[Error]\033[0m Bootloader requests not found, try rebooting.");
        cli();
        halt();
    }

    struct limine_memmap_response *memmap = memmap_request.response;
    uint64_t hhdm_offset = hhdm_request.response->offset;

    if (free_page_head != 0) {
        uint64_t phys_addr = free_page_head;
        free_page_head = *(uint64_t *)(phys_addr + hhdm_offset);
        memset((void *)(phys_addr + hhdm_offset), 0, PAGE_SIZE);
        return phys_addr;
    }

    for (; alloc_index < memmap->entry_count; alloc_index++) {
        struct limine_memmap_entry *entry = memmap->entries[alloc_index];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            alloc_offset = 0;
            continue;
        }

        if (alloc_offset + PAGE_SIZE <= entry->length) {
            uint64_t phys_addr = entry->base + alloc_offset;
            alloc_offset += PAGE_SIZE;

            memset((void *)(phys_addr + hhdm_offset), 0, PAGE_SIZE);
            return phys_addr;
        }
        
        alloc_offset = 0;
    }

    println("\033[31m[Error]\033[0m Out of physical RAM!");
    cleanup_pages();
    return 0;
}

size_t pmm_get_free_page_count(void) {
    if (!memmap_request.response) {
        return 0;
    }

    struct limine_memmap_response *memmap = memmap_request.response;
    size_t free_pages = 0;

    uint64_t current_free = free_page_head;
    uint64_t hhdm_offset = hhdm_request.response ? hhdm_request.response->offset : 0;
    while (current_free != 0) {
        free_pages++;
        if (hhdm_offset != 0) {
            current_free = *(uint64_t *)(current_free + hhdm_offset);
        } else {
            break;
        }
    }

    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            free_pages += entry->length / PAGE_SIZE;
        }
    }

    return free_pages;
}

static void pmm_free_page(uint64_t phys_addr, uint64_t hhdm_offset) {
    if (phys_addr == 0 || (phys_addr & 0xFFFULL) != 0) {
        return;
    }

    uint64_t *page = (uint64_t *)(phys_addr + hhdm_offset);
    *page = free_page_head;
    free_page_head = phys_addr;
}

static void free_user_table(uint64_t phys_addr, unsigned int level, uint64_t hhdm_offset) {
    uint64_t *table = (uint64_t *)(phys_addr + hhdm_offset);

    for (size_t i = 0; i < 512; i++) {
        uint64_t entry = table[i];
        if ((entry & PAGE_USER) == 0) {
            continue;
        }

        uint64_t child_phys = entry & PAGE_ADDRESS_MASK;
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

void cleanup_pages(void) {
    if (hhdm_request.response == NULL) {
        return;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *pml4 = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);
    uint64_t user_tables[256] = {0};

    for (size_t i = 0; i < 256; i++) {
        if ((pml4[i] & (PAGE_PRESENT | PAGE_USER)) == (PAGE_PRESENT | PAGE_USER)) {
            user_tables[i] = pml4[i] & PAGE_ADDRESS_MASK;
            pml4[i] = 0;
        }
    }

    write_cr3(read_cr3());

    for (size_t i = 0; i < 256; i++) {
        if (user_tables[i] != 0) {
            free_user_table(user_tables[i], 3, hhdm_offset);
        }
    }
}

uint64_t create_user_pml4(void) {
    if (hhdm_request.response == NULL) {
        return 0;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t new_pml4_phys = pmm_alloc_zeroed_page();
    if (new_pml4_phys == 0) {
        return 0;
    }

    uint64_t *new_pml4 = (uint64_t *)(new_pml4_phys + hhdm_offset);
    uint64_t *current_pml4 = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);

    for (size_t i = 256; i < 512; i++) {
        new_pml4[i] = current_pml4[i];
    }

    return new_pml4_phys;
}

void switch_user_pml4(uint64_t pml4_phys) {
    if (pml4_phys != 0) {
        write_cr3(pml4_phys & PAGE_ADDRESS_MASK);
    }
}

void destroy_user_pml4(uint64_t pml4_phys) {
    if (pml4_phys == 0 || hhdm_request.response == NULL) {
        return;
    }
    uint64_t current_cr3 = read_cr3() & PAGE_ADDRESS_MASK;
    uint64_t target_cr3 = pml4_phys & PAGE_ADDRESS_MASK;
    if (current_cr3 != target_cr3) {
        write_cr3(target_cr3);
    }
    cleanup_pages();
    if (current_cr3 != target_cr3) {
        write_cr3(current_cr3);
    }
    pmm_free_page(target_cr3, hhdm_request.response->offset);
}

bool suspend_user_pages(uint64_t *page_table_context) {
    if (page_table_context == NULL || hhdm_request.response == NULL) {
        return false;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *pml4 = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);
    for (size_t i = 0; i < 256; i++) {
        page_table_context[i] = pml4[i];
        pml4[i] = 0;
    }
    write_cr3(read_cr3());
    return true;
}

bool restore_user_pages(const uint64_t *page_table_context) {
    if (page_table_context == NULL || hhdm_request.response == NULL) {
        return false;
    }

    cleanup_pages();
    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *pml4 = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);
    for (size_t i = 0; i < 256; i++) {
        pml4[i] = page_table_context[i];
    }
    write_cr3(read_cr3());
    return true;
}

static inline uint64_t* get_or_alloc_table(uint64_t* table, size_t index, uint64_t hhdm_offset) {
    if (!(table[index] & PAGE_PRESENT)) {
        table[index] = pmm_alloc_zeroed_page() | PAGE_USER_FLAGS;
    } else {
        table[index] |= PAGE_USER_FLAGS;
    }
    return (uint64_t *)((table[index] & PAGE_ADDRESS_MASK) + hhdm_offset);
}

void map_user_page(uint64_t virt_addr, uint64_t phys_addr) {
    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *pml4 = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);

    uint64_t *pdpt = get_or_alloc_table(pml4, (virt_addr >> 39) & 0x1FF, hhdm_offset);
    uint64_t *pd   = get_or_alloc_table(pdpt, (virt_addr >> 30) & 0x1FF, hhdm_offset);
    uint64_t *pt   = get_or_alloc_table(pd,   (virt_addr >> 21) & 0x1FF, hhdm_offset);

    pt[(virt_addr >> 12) & 0x1FF] = (phys_addr & PAGE_ADDRESS_MASK) | PAGE_USER_FLAGS;
    
    invlpg(virt_addr);
}

bool protect_user_page(uint64_t virt_addr, bool present, bool write_access, bool execute_access) {
    if (hhdm_request.response == NULL) {
        return false;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *table = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);
    const size_t indices[] = {
        (virt_addr >> 39) & 0x1FF,
        (virt_addr >> 30) & 0x1FF,
        (virt_addr >> 21) & 0x1FF,
        (virt_addr >> 12) & 0x1FF
    };

    for (size_t level = 0; level < 3; level++) {
        uint64_t entry = table[indices[level]];
        if ((entry & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER)) {
            return false;
        }
        table = (uint64_t *)((entry & PAGE_ADDRESS_MASK) + hhdm_offset);
    }

    uint64_t *entry = &table[indices[3]];
    if ((*entry & PAGE_USER) == 0 || (*entry & PAGE_ADDRESS_MASK) == 0) {
        return false;
    }
    *entry &= ~(PAGE_PRESENT | PAGE_WRITABLE | PAGE_NO_EXECUTE);
    if (present) {
        *entry |= PAGE_PRESENT;
    }
    if (write_access) {
        *entry |= PAGE_WRITABLE;
    }
    if (!execute_access && (rdmsr(MSR_IA32_EFER) & EFER_NXE) != 0) {
        *entry |= PAGE_NO_EXECUTE;
    }
    invlpg(virt_addr);
    return true;
}

bool user_page_mapped(uint64_t virt_addr) {
    if (hhdm_request.response == NULL) {
        return false;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *table = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);
    const size_t indices[] = {
        (virt_addr >> 39) & 0x1FF,
        (virt_addr >> 30) & 0x1FF,
        (virt_addr >> 21) & 0x1FF,
        (virt_addr >> 12) & 0x1FF
    };

    for (size_t level = 0; level < 3; level++) {
        uint64_t entry = table[indices[level]];
        if ((entry & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER)) {
            return false;
        }
        table = (uint64_t *)((entry & PAGE_ADDRESS_MASK) + hhdm_offset);
    }

    uint64_t entry = table[indices[3]];
    return (entry & PAGE_USER) != 0 && (entry & PAGE_ADDRESS_MASK) != 0;
}

bool unmap_user_page(uint64_t virt_addr) {
    if (hhdm_request.response == NULL) {
        return false;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *table = (uint64_t *)((read_cr3() & PAGE_ADDRESS_MASK) + hhdm_offset);
    const size_t indices[] = {
        (virt_addr >> 39) & 0x1FF,
        (virt_addr >> 30) & 0x1FF,
        (virt_addr >> 21) & 0x1FF,
        (virt_addr >> 12) & 0x1FF
    };

    for (size_t level = 0; level < 3; level++) {
        uint64_t entry = table[indices[level]];
        if ((entry & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER)) {
            return false;
        }
        table = (uint64_t *)((entry & PAGE_ADDRESS_MASK) + hhdm_offset);
    }

    uint64_t *entry = &table[indices[3]];
    uint64_t phys_addr = *entry & PAGE_ADDRESS_MASK;
    if ((*entry & PAGE_USER) == 0 || phys_addr == 0) {
        return false;
    }
    *entry = 0;
    invlpg(virt_addr);
    pmm_free_page(phys_addr, hhdm_offset);
    return true;
}

void setup_ring3_memory(uint64_t user_code_virt, uint64_t user_stack_virt, const uint8_t *user_code, size_t code_size) {
    uint64_t hhdm_offset = hhdm_request.response->offset;

    for (size_t offset = 0; offset < code_size; offset += PAGE_SIZE) {
        uint64_t code_phys = pmm_alloc_zeroed_page();
        size_t bytes_to_copy = (code_size - offset > PAGE_SIZE) ? PAGE_SIZE : (code_size - offset);
        
        memcpy((void *)(code_phys + hhdm_offset), user_code + offset, bytes_to_copy);
        map_user_page(user_code_virt + offset, code_phys);
    }

    uint64_t stack_phys = pmm_alloc_zeroed_page();
    if (!stack_phys) {
        println("\033[31m[Error]\033[0m Out of physical RAM while setting up Ring 3 stack!");
        return;
    }
    map_user_page(user_stack_virt, stack_phys);

    uint64_t user_data_virt = user_stack_virt - DATA_AMOUNT; 
    for (int i = 0; i < 2; i++) {
        uint64_t data_phys = pmm_alloc_zeroed_page();
        map_user_page(user_data_virt + (uint64_t)(i * PAGE_SIZE), data_phys);
    }
}