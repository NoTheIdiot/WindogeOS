#include <stdint.h>
#include <stddef.h>
#include <system.h>
#include <bool.h>
#include <dogeio.h>

#define HEAP_MAGIC 0xDEADBEEFCAFEBABEULL
#define MIN_ALIGN  16

typedef struct block_header {
    uint64_t size;
    bool is_free;
    struct block_header *next;
    struct block_header *prev;
} block_header;

typedef struct alloc_header {
    uint64_t magic;
    block_header *block;
} alloc_header;

static block_header *heap_head = NULL;

void init_kheap(void *start_addr, uint64_t total_size) {
    heap_head = (block_header *)start_addr;
    heap_head->size = total_size;
    heap_head->is_free = true;
    heap_head->next = NULL;
    heap_head->prev = NULL;
}

static inline uint64_t align_up(uint64_t val, uint64_t align) {
    return (val + align - 1) & ~(align - 1);
}

static void split_block(block_header *block, uint64_t needed_size) {
    uint64_t min_split = sizeof(block_header) + sizeof(alloc_header) + MIN_ALIGN;
    if (block->size >= needed_size + min_split) {
        block_header *new_block = (block_header *)((uint8_t *)block + needed_size);
        new_block->size = block->size - needed_size;
        new_block->is_free = true;
        new_block->next = block->next;
        new_block->prev = block;

        if (block->next) {
            block->next->prev = new_block;
        }
        block->next = new_block;
        block->size = needed_size;
    }
}

void *kmalloc_aligned(uint64_t size, uint64_t align) {
    if (size == 0) return NULL;
    if (align < MIN_ALIGN) align = MIN_ALIGN;
    if ((align & (align - 1)) != 0) return NULL;

    block_header *curr = heap_head;
    while (curr != NULL) {
        if (curr->is_free) {
            uint64_t block_addr = (uint64_t)curr;
            uint64_t raw_data_start = block_addr + sizeof(block_header) + sizeof(alloc_header);
            uint64_t aligned_data_start = align_up(raw_data_start, align);
            uint64_t total_needed = (aligned_data_start - block_addr) + size;

            if (curr->size >= total_needed) {
                split_block(curr, total_needed);
                curr->is_free = false;

                alloc_header *alloc_hdr = (alloc_header *)(aligned_data_start - sizeof(alloc_header));
                alloc_hdr->magic = HEAP_MAGIC;
                alloc_hdr->block = curr;

                return (void *)aligned_data_start;
            }
        }
        curr = curr->next;
    }

    return NULL;
}

void *kmalloc(uint64_t size) {
    return kmalloc_aligned(size, MIN_ALIGN);
}

void kfree(void *ptr) {
    if (!ptr) return;

    alloc_header *alloc_hdr = (alloc_header *)((uint8_t *)ptr - sizeof(alloc_header));
    if (alloc_hdr->magic != HEAP_MAGIC) {
        return;
    }

    block_header *block = alloc_hdr->block;
    alloc_hdr->magic = 0;
    block->is_free = true;

    if (block->next && block->next->is_free) {
        block->size += block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    if (block->prev && block->prev->is_free) {
        block->prev->size += block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}
