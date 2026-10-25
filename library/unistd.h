#ifndef _UNISTD_H
#define _UNISTD_H

#include "stdint.h"
#include "stddef.h"
#include "internal/syscalls.h"
#include "bool.h"
#include "dogeio.h"
#include "dogelib.h"

typedef long ssize_t;
typedef long off_t;
typedef int pid_t;

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     4
#define O_TRUNC     8
#define O_APPEND    16

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

static const char* __active_fd_path = (const char*)0;

static inline int open(const char *pathname, int flags, ...) {
    if (flags & O_CREAT) {
        create_file(pathname);
    }
    if (file_exists(pathname)) {
        __active_fd_path = pathname;
        return 3;
    }
    return -1;
}

static inline ssize_t read(int fd, void *buf, size_t count) {
    if (fd == 0) {
        return 0;
    }
    if (__active_fd_path) {
        return (ssize_t)read_file(__active_fd_path, buf, count);
    }
    return -1;
}

static inline ssize_t write(int fd, const void *buf, size_t count) {
    if (fd == 1 || fd == 2) {
        print((const char *)buf);
        return (ssize_t)count;
    }
    if (__active_fd_path) {
        return (ssize_t)write_file_raw(__active_fd_path, (const uint8_t *)buf, count);
    }
    return -1;
}

static inline int close(int fd) {
    (void)fd;
    __active_fd_path = (const char*)0; 
    return 0;
}

static inline off_t lseek(int fd, off_t offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    return 0;
}

static inline pid_t getpid(void) {
    return (pid_t)doge_get_pid();
}

static inline int brk(void *addr) {
    (void)addr;
    return 0;
}

static inline char *getcwd(char *buf, size_t size) {
    long res = (long)get_cwd(buf, size);
    if (res < 0) return (char*)0;
    return buf;
}

static inline int fstat(int fd, dogec_stat_t *statbuf) {
    (void)fd;
    if (__active_fd_path) {
        return (int)stat(__active_fd_path, statbuf);
    }
    return -1;
}

static inline int stat_posix(const char *pathname, dogec_stat_t *statbuf) {
    return (int)stat(pathname, statbuf);
}

#endif
