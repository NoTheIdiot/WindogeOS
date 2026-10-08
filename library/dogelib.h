#ifndef DOGELIB_H
#define DOGELIB_H

#include <stdint.h>
#include "internal/syscalls.h"
#include "dogeio.h"

static inline uint64_t shell(char* command)  {
    return syscall_1(SHELL, (uint64_t)command);
}

static inline uint64_t exec(char* command)  {
    return syscall_1(EXEC, (uint64_t)command);
}

#endif