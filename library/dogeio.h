#ifndef USER_DOGEIO_H
#define USER_DOGEIO_H

#include <stdint.h>
#include "internal/syscalls.h"

typedef struct {
    uint64_t size;
    uint64_t is_dir;
    uint64_t exists;
} dogec_stat_t;

static inline uint64_t read_file(const char *path, void *buffer, uint64_t size) {
    return syscall_3(READ_FILE, (uint64_t)path, (uint64_t)buffer, size);
}

static inline uint64_t write_file(const char *path, const char *text) {
    return syscall_2(WRITE_FILE, (uint64_t)path, (uint64_t)text);
}

static inline uint64_t append_file(const char *path, const void *buffer,
                                   uint64_t size) {
    return syscall_3(APPEND_FILE, (uint64_t)path, (uint64_t)buffer, size);
}

static inline uint64_t create_file(const char *path) {
    return syscall_1(CREATE_FILE, (uint64_t)path);
}

static inline uint64_t delete_file(const char *path) {
    return syscall_1(DELETE_FILE, (uint64_t)path);
}

static inline uint64_t create_dir(const char *path) {
    return syscall_1(CREATE_DIR, (uint64_t)path);
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

static inline uint64_t rename_file(const char *path, const char *new_path) {
    return syscall_2(RENAME_FILE, (uint64_t)path, (uint64_t)new_path);
}

static inline uint64_t file_exists(const char *path) {
    return syscall_1(FILE_EXISTS, (uint64_t)path);
}

static inline uint64_t change_dir(const char *path) {
    return syscall_1(CHANGE_DIR, (uint64_t)path);
}

static inline uint64_t get_cwd(char *buffer, uint64_t capacity) {
    return syscall_2(GET_CWD, (uint64_t)buffer, capacity);
}

static inline uint64_t delete_last_line(const char *path) {
    return syscall_1(DELETE_LAST_LINE, (uint64_t)path);
}

static inline uint64_t copy_file(const char *source, const char *destination) {
    return syscall_2(COPY_FILE, (uint64_t)source, (uint64_t)destination);
}

static inline uint64_t print(const char *text) {
    return syscall_1(PRINT, (uint64_t)text);
}

static inline uint64_t println(const char *text) {
    return syscall_1(PRINTLN, (uint64_t)text);
}

static inline uint64_t clear(void) {
    return syscall_0(CLEAR);
}

static inline uint64_t input(const char *prompt, char *buffer,
                             uint64_t capacity) {
    return syscall_3(INPUT, (uint64_t)prompt, (uint64_t)buffer, capacity);
}

static inline uint64_t get_key(void) {
    return syscall_0(GET_KEY);
}

static inline uint64_t print_at(const char *text, uint32_t x, uint32_t y,
                                uint32_t color) {
    return syscall_4(PRINT_AT, (uint64_t)text, x, y, color);
}

static inline uint64_t text_color(uint32_t color) {
    return syscall_1(TEXT_COLOR, color);
}

static inline uint64_t background_color(uint32_t color) {
    return syscall_1(BACKGROUND_COLOR, color);
}

static inline uint64_t sys_exit(uint64_t status) {
    return syscall_1(SYS_EXIT, status);
}

#endif
