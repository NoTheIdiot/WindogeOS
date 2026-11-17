#ifndef DOGEIO_H
#define DOGEIO_H

#include "stdint.h"
#include "stddef.h"
#include "root/syscalls.h"
#include "bool.h"

// types
typedef struct {
    char name[256];
    uint32_t size;
    bool is_dir;
} fs_t;

static inline void exit(void) {
    syscall_0(Process_Exit);
}

static inline int fs_read(const char *path, char *buf, size_t len) {
    return (int)syscall_3(ReadFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int fs_write(const char *path, const char *buf, size_t len) {
    return (int)syscall_3(WriteFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int fs_read_raw(const char *path, uint8_t *buf, size_t len) {
    return (int)syscall_3(ReadFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int fs_write_raw(const char *path, uint8_t *buf, size_t len) {
    return (int)syscall_3(WriteFile, (uint64_t)path, (uint64_t)buf, (uint64_t)len);
}

static inline int fs_create(const char *path) {
    return (int)syscall_1(CreateFile, (uint64_t)path);
}

static inline int fs_mkdir(const char* path) {
    return (int)syscall_1(CreateDir, (uint64_t)path);
}

static inline int fs_delete(const char* path) {
    return (int)syscall_1(DeleteThing, (uint64_t)path);
}

static inline int fs_exists(const char* path) {
    return (int)syscall_1(Exists, (uint64_t)path);
}

static inline int fs_list(char* path, fs_t* buffer, int max) {
    return (int)syscall_3(ListDirectory, (uint64_t)path, (uint64_t)buffer, (uint64_t)max);
}

static inline int fs_chdir(char* path) {
    return (int)syscall_1(ChangeDirectory, (uint64_t)path);
}

static inline int fs_get_cwd(char *buf, size_t len) {
    return (int)syscall_2(CurrentDirectory, (uint64_t)buf, (uint64_t)len);
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
