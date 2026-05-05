// include files
#include <stddef.h>
#include <stdint.h>
#include "../helper/dogeio.h"
#include "multiboot.h"

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
