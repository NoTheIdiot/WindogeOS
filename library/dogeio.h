#ifndef DOGEIO_H
#define DOGEIO_H

#include "stdint.h"
#include "stddef.h"
#include "root/syscalls.h"

static inline void exit(void) {
    syscall_0(Process_Exit);
}

static inline int read_file(const char *path, char *buf, size_t len) {
    return (int)syscall_3(ReadFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int write_file(const char *path, const char *buf, size_t len) {
    return (int)syscall_3(WriteFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int create_file(const char *path) {
    return (int)syscall_1(CreateFile, (uint64_t)path);
}

static inline int print(const char *str) {
    return (int)syscall_1(Print, (uint64_t)str);
}

static inline int println(const char *str) {
    if (print(str) < 0) return -1;
    return print("\n");
}

static inline void clear(void) {
    print("\033[2J\033[H");
}

static inline int input(const char *prompt, char *buf, size_t size) {
    return (int)syscall_3(Input, (uint64_t)prompt, (uint64_t)buf, (uint64_t)size);
}

#endif
