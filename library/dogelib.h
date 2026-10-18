#ifndef DOGELIB_H
#define DOGELIB_H

#include "stdint.h"
#include "stddef.h"
#include "internal/syscalls.h"
#include "dogeio.h"

static inline uint64_t shell(char* command)  {
    return syscall_1(SHELL, (uint64_t)command);
}

static inline uint64_t exec(char* command)  {
    return syscall_1(EXEC, (uint64_t)command);
}

static inline uint64_t exec_args(const char *path, uint64_t argc,
                                char *const argv[]) {
    return syscall_3(EXEC_ARGS, (uint64_t)path, argc, (uint64_t)argv);
}

static inline uint64_t exec_elf(const char *path) {
    return syscall_1(EXEC_ELF, (uint64_t)path);
}

static inline uint64_t doge_get_pid(void) {
    return syscall_0(GET_PID);
}

static inline uint64_t doge_get_uid(void) {
    return syscall_0(GET_UID);
}

static inline uint64_t doge_get_gid(void) {
    return syscall_0(GET_GID);
}

static inline uint64_t doge_get_euid(void) {
    return syscall_0(GET_EUID);
}

static inline uint64_t doge_get_egid(void) {
    return syscall_0(GET_EGID);
}

static inline uint64_t get_user(void) {
    return syscall_0(GET_USER);
}

#define HEAP_MAGIC 0x444F474554485058ULL

typedef struct block_header {
    uint64_t magic;
    size_t size;
    int free;
    struct block_header* next;
    struct block_header* prev;
} block_header_t;

static block_header_t* heap_start = (block_header_t*)0;
static size_t total_mapped_size = 0;

#define MAP_USER_MEMORY 28
#define USER_MEMORY_READ 0x1
#define USER_MEMORY_WRITE 0x2
#define PAGE_SIZE 4096

static inline void* extend_heap(size_t size) {
    size_t pages = (size + sizeof(block_header_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t map_size = pages * PAGE_SIZE;

    void* ptr = (void*)syscall_2(MAP_USER_MEMORY, map_size, USER_MEMORY_READ | USER_MEMORY_WRITE);
    if (!ptr || ptr == (void*)-1) {
        return (void*)0;
    }

    block_header_t* new_block = (block_header_t*)ptr;
    new_block->magic = HEAP_MAGIC;
    new_block->size = map_size - sizeof(block_header_t);
    new_block->free = 1;
    new_block->next = (block_header_t*)0;
    new_block->prev = (block_header_t*)0;

    if (!heap_start) {
        heap_start = new_block;
    } else {
        block_header_t* curr = heap_start;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_block;
        new_block->prev = curr;
    }

    total_mapped_size += map_size;
    return new_block;
}

static inline void* malloc(size_t size) {
    if (size == 0) return (void*)0;

    size_t aligned_size = (size + 7) & (size_t)(~7);

    if (!heap_start) {
        if (!extend_heap(aligned_size)) {
            return (void*)0;
        }
    }

    block_header_t* curr = heap_start;
    while (curr) {
        if (curr->magic == HEAP_MAGIC && curr->free && curr->size >= aligned_size) {
            if (curr->size >= aligned_size + sizeof(block_header_t) + 8) {
                block_header_t* next_block = (block_header_t*)((char*)curr + sizeof(block_header_t) + aligned_size);
                next_block->magic = HEAP_MAGIC;
                next_block->size = curr->size - aligned_size - sizeof(block_header_t);
                next_block->free = 1;
                next_block->next = curr->next;
                next_block->prev = curr;

                if (curr->next) {
                    curr->next->prev = next_block;
                }
                curr->next = next_block;
                curr->size = aligned_size;
            }
            curr->free = 0;
            return (void*)((char*)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }

    if (!extend_heap(aligned_size)) {
        return (void*)0;
    }

    return malloc(size);
}

static inline void free(void* ptr) {
    if (!ptr) return;

    block_header_t* block = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    if (block->magic != HEAP_MAGIC) return;

    block->free = 1;

    if (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(block_header_t) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

#endif