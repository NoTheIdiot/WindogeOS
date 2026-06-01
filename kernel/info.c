// include files
#include <stddef.h>
#include <stdint.h>
#include "dogeio.h"
#include "string.h"
#include "multiboot.h"
#include "file.h"

#define PAGE_SIZE 4096
size_t total_pages = 0;
size_t used_pages = 0;
uint8_t* mem_bitmap = NULL;
#define BITMAP_MAX_SIZE 1024*1024
uint8_t bitmap_storage[BITMAP_MAX_SIZE];

extern char* such_windoge_version;
extern char* such_windoge_version_short;
extern char* boot_time;

// im sure im going to use this alot
void *memset(void *ptr, int value, size_t num) {
    unsigned char *p = ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

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

int mem_get_used_bytes() {
    return used_pages * PAGE_SIZE;
}

int mem_get_total_bytes() {
    return total_pages * PAGE_SIZE;
}

void mem_init(multiboot_info_t* mbi) {
    uint32_t total_mb = info_get_ram_amount(mbi);
    uint64_t total_bytes = (uint64_t)total_mb * 1024 * 1024;

    total_pages = total_bytes / PAGE_SIZE;

    size_t bitmap_size = (total_pages + 7) / 8;
    if (bitmap_size > BITMAP_MAX_SIZE) bitmap_size = BITMAP_MAX_SIZE;

    mem_bitmap = bitmap_storage;
    memset(mem_bitmap, 0, bitmap_size);
    (void)mbi;
}

void system_sysinfo() {
    mem_calculate_usage();
    int file_count = 0;
    char count_str[16]; // Declared out here so the entire function can see it

    // Fixed the missing variable 'i' declaration
    for (int i = 0; i < 16; i++) {
        vfs_file* check_file = file_system[i];
        if (check_file->name[0] != '\0') {
            file_count++;
        }
    }
    
    // Convert to string ONCE after counting is completely done
    string_itoa(file_count, count_str);

    char cpu_name[64];
    info_get_cpu_name(cpu_name);

    int used_mb = mem_get_used_bytes() / (1024 * 1024);
    int total_mb = mem_get_total_bytes() / (1024 * 1024);

    char used_str[32], total_str[32];
    string_itoa(used_mb, used_str);
    string_itoa(total_mb, total_str);

    dogeio_println("System information of your wow system:\n");
    dogeio_print("WindogeOS Version: ");
    dogeio_println(such_windoge_version);
    dogeio_print("CPU: ");
    dogeio_println(cpu_name);
    dogeio_print("RAM: ");
    dogeio_print(used_str);
    dogeio_print(" MB / ");
    dogeio_print(total_str);
    dogeio_println(" MB");
    dogeio_print("Boot Time: ");
    dogeio_println(boot_time);
    dogeio_print("File amount: ");
    dogeio_println(count_str);
}
