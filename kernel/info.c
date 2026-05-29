// include files
#include <stddef.h>
#include <stdint.h>
#include "dogeio.h"
#include "multiboot.h"

#define PAGE_SIZE 4096
size_t total_pages = 0;
size_t used_pages = 0;
uint8_t* mem_bitmap = NULL;

// get the CPU name
void info_get_cpu_name(char *buffer) {
    uint32_t eax, ebx, ecx, edx;

    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        __asm__ volatile (
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(leaf)
        );

        ((uint32_t *)buffer)[0] = eax;
        ((uint32_t *)buffer)[1] = ebx;
        ((uint32_t *)buffer)[2] = ecx;
        ((uint32_t *)buffer)[3] = edx;

        buffer += 16;
    }

    *buffer = '\0';
}

// get the ram amount
uint32_t info_get_ram_amount(multiboot_info_t* mbi) {
    struct multiboot_mmap_entry* mmap = (struct multiboot_mmap_entry*)mbi->mmap_addr;
    uint32_t mmap_end = mbi->mmap_addr + mbi->mmap_length;
    uint64_t total_bytes = 0;

    while ((uint32_t)mmap < mmap_end) {
        if (mmap->type == 1) {
            total_bytes += mmap->len;
        }
        mmap = (struct multiboot_mmap_entry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }

    return (uint32_t)(total_bytes / 1024 / 1024);
}


int bitmap_is_bit_set(size_t page_index) {
    if (page_index >= total_pages) {
        return 0; // out of bounds, you don't have more ram.
    }

    size_t byte_index = page_index / 8;
    size_t bit_index = page_index % 8;

    return (mem_bitmap[byte_index] & (1 << bit_index)) != 0;
}

void bitmap_set_bit(size_t page_index) {
    size_t byte_index = page_index / 8;
    size_t bit_index = page_index % 8;
    
    mem_bitmap[byte_index] |= (1 << bit_index);
}

void bitmap_clear_bit(size_t page_index) {
    size_t byte_index = page_index / 8;
    size_t bit_index = page_index % 8;
    
    mem_bitmap[byte_index] &= ~(1 << bit_index);
}

void mem_calculate_usage() {
    used_pages = 0;
    for (size_t i = 0; i < total_pages; i++) {
        if (bitmap_is_bit_set(i)) {
            used_pages++;
        }
    }
}

size_t mem_get_used_bytes() {
    return used_pages * PAGE_SIZE;
}

size_t mem_get_total_bytes() {
    return total_pages * PAGE_SIZE;
}
