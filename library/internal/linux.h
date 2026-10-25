#ifndef LINUX_H
#define LINUX_H

/* Notes for anything here:
   If you want to steal my code, you can and i encourage that.
   tip: do not copy the linux syscalls they are here so i dont
   have to port shit myself */

#define LINUX_SYS_READ          0
#define LINUX_SYS_WRITE         1
#define LINUX_SYS_OPEN          2
#define LINUX_SYS_CLOSE         3
#define LINUX_SYS_STAT          4
#define LINUX_SYS_FSTAT         5
#define LINUX_SYS_LSEEK         8
#define LINUX_SYS_MMAP          9
#define LINUX_SYS_MPROTECT      10
#define LINUX_SYS_MUNMAP        11
#define LINUX_SYS_BRK           12
#define LINUX_SYS_PREAD64       17
#define LINUX_SYS_ACCESS        21
#define LINUX_SYS_GETPID        39
#define LINUX_SYS_EXIT          60
#define LINUX_SYS_UNAME         63
#define LINUX_SYS_GETTIMEOFDAY  96
#define LINUX_SYS_GETUID        102
#define LINUX_SYS_GETGID        104
#define LINUX_SYS_GETEUID       107
#define LINUX_SYS_GETEGID       108
#define LINUX_SYS_TIME          201
#define LINUX_SYS_CLOCK_GETTIME 228
#define LINUX_SYS_EXIT_GROUP    231
#define LINUX_SYS_GETCWD        79
#define LINUX_SYS_OPENAT        257
#define LINUX_SYS_NEWFSTATAT    262

#define LINUX_O_WRONLY   0x0001
#define LINUX_O_RDWR     0x0002
#define LINUX_O_CREAT    0x0040
#define LINUX_O_TRUNC    0x0200
#define LINUX_O_APPEND   0x0400
#define LINUX_AT_FDCWD   ((uint64_t)-100)
#define LINUX_AT_SYMLINK_NOFOLLOW 0x0100
#define LINUX_AT_EMPTY_PATH       0x1000

#define LINUX_SEEK_SET   0
#define LINUX_SEEK_CUR   1
#define LINUX_SEEK_END   2

#define LINUX_PROT_WRITE  0x2
#define LINUX_PROT_EXEC   0x4
#define LINUX_PROT_NONE   0x0
#define LINUX_MAP_FIXED   0x10
#define LINUX_MAP_ANON    0x20
#define LINUX_MAP_PRIVATE 0x02
#define LINUX_MAP_SHARED  0x01

#define LINUX_EBADF     9
#define LINUX_EIO       5
#define LINUX_EFAULT    14
#define LINUX_EINVAL    22
#define LINUX_EMFILE    24
#define LINUX_ENOENT    2
#define LINUX_ENOMEM    12
#define LINUX_EISDIR    21
#define LINUX_EACCES    13
#define LINUX_ESPIPE    29
#define LINUX_ENOSYS    38
#define LINUX_EOVERFLOW 75
#define LINUX_EFBIG     27

#define USER_IO_LIMIT         65536ULL
#define USER_PATH_LIMIT       256
#define USER_PAGE_SIZE        4096ULL
#define USER_HEAP_BASE        0x0000600000000000ULL
#define USER_MMAP_BASE        0x0000700000000000ULL
#define USER_MMAP_LIMIT       0x00007F0000000000ULL
#define USER_ADDRESS_LIMIT    0x0000800000000000ULL
#define LINUX_FD_LIMIT        32
#define LINUX_FILE_IO_LIMIT   65536ULL
#define LINUX_CLOCK_REALTIME  0
#define LINUX_CLOCK_MONOTONIC 1

#endif