#include <stddef.h>
#include <stdint.h>
#include <boot/limine.h>
#include <dogeio.h>

extern volatile struct limine_memmap_request memmap_request;

void* kmalloc(size_t size) {
    static uintptr_t current_heap_ptr = 0;

    if (current_heap_ptr == 0) {
        struct limine_memmap_response* response = memmap_request.response;
        if (response != NULL && response->entry_count > 0) {
            for (uint64_t i = 0; i < response->entry_count; i++) {
                struct limine_memmap_entry* entry = response->entries[i];
                if (entry->type == LIMINE_MEMMAP_USABLE) {
                    current_heap_ptr = (uintptr_t)entry->base;
                    break;
                }
            }
        }
        
        if (current_heap_ptr == 0) {
            extern uint8_t endkernel;
            current_heap_ptr = (uintptr_t)&endkernel;
        }
    }

    current_heap_ptr = (current_heap_ptr + 15) & ~15UL;
    
    void* alloc = (void*)current_heap_ptr;
    current_heap_ptr += size;

    return alloc;
}

void kfree(void* ptr) {
    (void)ptr;
}
