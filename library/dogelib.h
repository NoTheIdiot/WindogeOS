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

#endif