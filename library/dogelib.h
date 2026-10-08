#ifndef DOGELIB_H
#define DOGELIB_H

#include <stdint.h>
#include "internal/syscalls.h"

typedef struct {
    uint64_t size;
    uint64_t is_dir;
    uint64_t exists;
} dogec_stat_t;

static inline uint64_t shell(char* command)  {
    return syscall_1(SHELL, (uint64_t)command);
}

static inline uint64_t exec(char* command)  {
    return syscall_1(EXEC, (uint64_t)command);
}

static inline uint64_t mkdir_recursive(const char *path) {
    return syscall_1(MKDIR_RECURSIVE, (uint64_t)path);
}

static inline uint64_t get_time(uint64_t *out_time) {
    return syscall_1(GET_TIME, (uint64_t)out_time);
}

static inline uint64_t stat(const char *path, dogec_stat_t *out_stat) {
    return syscall_2(STAT, (uint64_t)path, (uint64_t)out_stat);
}

#endif