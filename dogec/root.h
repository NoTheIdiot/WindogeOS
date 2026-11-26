#ifndef ROOT_H
#define ROOT_H

#include "stdint.h"
#include "bool.h"
#include <stdarg.h>

enum {
    TEXT = 1,
    FS   = 2,
    DOGE = 3,
    MEM  = 4,
    IPC  = 5,
    PROC = 6,
};

enum {
    MEM_SBRK = 0x01,
    MEM_MAP  = 0x02
};

enum {
    TEXT_PRINTF = 0x01,
    TEXT_GETKEY = 0x02,
    TEXT_INPUT  = 0x03
};

enum {
    DOGE_EXIT     = 0x01,
    DOGE_SLEEP    = 0x02,
    DOGE_UPTIME   = 0x03,
    DOGE_GET_TIME = 0x04,
    DOGE_BEEP     = 0x05,
    DOGE_RAND     = 0x06,
    DOGE_REBOOT   = 0x07,
    DOGE_SHUTDOWN = 0x08
};

enum {
    FS_READ   = 0x01,
    FS_WRITE  = 0x02,
    FS_CREATE = 0x03,
    FS_DELETE = 0x04,
    FS_STAT   = 0x05,
    FS_LISTDIR = 0x06,
    FS_CHDIR  = 0x07,
    FS_GETDIR = 0x08
};

enum {
    IPC_SEND = 0x01,
    IPC_RECV = 0x02
};

enum {
    PROC_RUN  = 0x01,
    PROC_EXIT = 0x02
};

typedef struct {
    uint64_t size;
} doge_fs_stat_t;

#define ERR_UNKNOWN_SYSCALL ((uint64_t)-1)
#define ERR_UNKNOWN_OPCODE  ((uint64_t)-2)
#define ERR_BAD_PTR         ((uint64_t)-3)
#define ERR_INVAL           ((uint64_t)-4)

static inline uint64_t syscall_0(uint64_t num) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        :
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_1(uint64_t num, uint64_t arg1) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_4(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    register uint64_t r10_arg __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10_arg)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_5(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    register uint64_t r10_arg __asm__("r10") = arg4;
    register uint64_t r8_arg  __asm__("r8")  = arg5;
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10_arg), "r"(r8_arg)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_6(uint64_t num, uint64_t arg1, uint64_t arg2,
                                 uint64_t arg3, uint64_t arg4, uint64_t arg5,
                                 uint64_t arg6) {
    register uint64_t r10_arg __asm__("r10") = arg4;
    register uint64_t r8_arg  __asm__("r8")  = arg5;
    register uint64_t r9_arg  __asm__("r9")  = arg6;
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10_arg), "r"(r8_arg), "r"(r9_arg)
        : "rcx", "r11", "memory"
    );
    return num;
}


typedef struct {
    char name[256];
    uint32_t size;
    uint8_t is_dir;
    uint32_t first_cluster;
    uint8_t perm;
} fs_t;

static inline int _root_vsnprintf(char *buf, uint64_t size, const char *fmt, va_list args) {
    if (!buf || size == 0 || !fmt) return -1;

    uint64_t pos = 0;
    #define EMIT(c) do { if (pos + 1 < size) buf[pos] = (c); pos++; } while (0)

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            EMIT(*p);
            continue;
        }
        p++;
        if (!*p) break;

        bool is_long = false;
        if (*p == 'l') {
            is_long = true;
            p++;
            if (*p == 'l') p++;
        } else if (*p == 'z') {
            is_long = true;
            p++;
        }

        switch (*p) {
            case '%':
                EMIT('%');
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                EMIT(c);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) EMIT(*s++);
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = is_long ? va_arg(args, int64_t) : (int64_t)va_arg(args, int);
                uint64_t uval;
                if (val < 0) {
                    EMIT('-');
                    uval = (uint64_t)(-(val + 1)) + 1;
                } else {
                    uval = (uint64_t)val;
                }
                char tmp[24];
                int i = 0;
                do { tmp[i++] = (char)('0' + (uval % 10)); uval /= 10; } while (uval);
                while (i > 0) EMIT(tmp[--i]);
                break;
            }
            case 'u': {
                uint64_t uval = is_long ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
                char tmp[24];
                int i = 0;
                do { tmp[i++] = (char)('0' + (uval % 10)); uval /= 10; } while (uval);
                while (i > 0) EMIT(tmp[--i]);
                break;
            }
            case 'x':
            case 'X':
            case 'p': {
                const char *digits = (*p == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                uint64_t uval = (*p == 'p' || is_long)
                                ? va_arg(args, uint64_t)
                                : (uint64_t)va_arg(args, unsigned int);
                if (*p == 'p') {
                    EMIT('0');
                    EMIT('x');
                }
                char tmp[24];
                int i = 0;
                do { tmp[i++] = digits[uval & 0xF]; uval >>= 4; } while (uval);
                while (i > 0) EMIT(tmp[--i]);
                break;
            }
            default:
                EMIT('%');
                EMIT(*p);
                break;
        }
    }

    #undef EMIT
    uint64_t out_len = (pos < size) ? pos : (size - 1);
    buf[out_len] = '\0';
    return (int)out_len;
}

static inline uint64_t root_console(uint32_t opcode, ...) {
    va_list args;
    va_start(args, opcode);
    uint64_t ret = ERR_UNKNOWN_OPCODE;

    switch (opcode) {
        case TEXT_PRINTF: {
            const char *fmt = va_arg(args, const char *);
            char buf[512];
            int len = _root_vsnprintf(buf, sizeof(buf), fmt, args);
            if (len < 0) {
                ret = ERR_INVAL;
                break;
            }
            ret = syscall_3(TEXT, TEXT_PRINTF, (uint64_t)buf, (uint64_t)len);
            break;
        }
        case TEXT_GETKEY: {
            ret = syscall_1(TEXT, TEXT_GETKEY);
            break;
        }
        case TEXT_INPUT: {
            char *buf = va_arg(args, char *);
            uint64_t max_len = va_arg(args, uint64_t);
            ret = syscall_3(TEXT, TEXT_INPUT, (uint64_t)buf, max_len);
            break;
        }
        default:
            break;
    }

    va_end(args);
    return ret;
}

static inline uint64_t root_fs(uint32_t opcode, ...) {
    va_list args;
    va_start(args, opcode);
    uint64_t ret = ERR_UNKNOWN_OPCODE;

    switch (opcode) {
        case FS_READ: {
            const char *filename = va_arg(args, const char *);
            void *buf = va_arg(args, void *);
            uint64_t size = va_arg(args, uint64_t);
            ret = syscall_4(FS, FS_READ, (uint64_t)filename, (uint64_t)buf, size);
            break;
        }
        case FS_WRITE: {
            const char *filename = va_arg(args, const char *);
            const void *buf = va_arg(args, const void *);
            uint64_t size = va_arg(args, uint64_t);
            ret = syscall_4(FS, FS_WRITE, (uint64_t)filename, (uint64_t)buf, size);
            break;
        }
        case FS_CREATE: {
            const char *filename = va_arg(args, const char *);
            ret = syscall_2(FS, FS_CREATE, (uint64_t)filename);
            break;
        }
        case FS_DELETE: {
            const char *filename = va_arg(args, const char *);
            ret = syscall_2(FS, FS_DELETE, (uint64_t)filename);
            break;
        }
        case FS_STAT: {
            const char *filename = va_arg(args, const char *);
            doge_fs_stat_t *st = va_arg(args, doge_fs_stat_t *);
            ret = syscall_3(FS, FS_STAT, (uint64_t)filename, (uint64_t)st);
            break;
        }

        case FS_LISTDIR: {
            const char *path = va_arg(args, const char *);
            fs_t *entries = va_arg(args, fs_t *);
            uint64_t max_entries = va_arg(args, uint64_t);

            ret = syscall_4(FS, FS_LISTDIR, (uint64_t)path,
                            (uint64_t)entries, max_entries);
            break;
        }

        case FS_CHDIR: {
            const char *path = va_arg(args, const char *);

            ret = syscall_2(FS, FS_CHDIR, (uint64_t)path);
            break;
        }

        case FS_GETDIR: {
            char *buffer = va_arg(args, char *);
            uint64_t size = va_arg(args, uint64_t);

            ret = syscall_3(FS, FS_GETDIR, (uint64_t)buffer, size);
            break;
        }
        default:
            break;
    }

    va_end(args);
    return ret;
}

static inline uint64_t root_doge(uint32_t opcode, ...) {
    va_list args;
    va_start(args, opcode);
    uint64_t ret = ERR_UNKNOWN_OPCODE;

    switch (opcode) {
        case DOGE_EXIT: {
            ret = syscall_1(DOGE, DOGE_EXIT);
            break;
        }
        case DOGE_SLEEP: {
            uint64_t time = va_arg(args, uint64_t);
            ret = syscall_2(DOGE, DOGE_SLEEP, time);
            break;
        }
        case DOGE_UPTIME: {
            ret = syscall_1(DOGE, DOGE_UPTIME);
            break;
        }
        case DOGE_GET_TIME: {
            char *buf = va_arg(args, char *);
            uint64_t size = va_arg(args, uint64_t);
            ret = syscall_3(DOGE, DOGE_GET_TIME, (uint64_t)buf, size);
            break;
        }
        case DOGE_BEEP: {
            uint64_t hz   = va_arg(args, uint64_t);
            uint64_t time = va_arg(args, uint64_t);
            ret = syscall_3(DOGE, DOGE_BEEP, hz, time);
            break;
        }
        case DOGE_RAND: {
            uint64_t *out = va_arg(args, uint64_t *);
            ret = syscall_2(DOGE, DOGE_RAND, (uint64_t)out);
            break;
        }

        case DOGE_REBOOT: {
            ret = syscall_1(DOGE, DOGE_REBOOT);
            break;
        }

        case DOGE_SHUTDOWN: {
            ret = syscall_1(DOGE, DOGE_SHUTDOWN);
            break;
        }

        default:
            break;
    }

    va_end(args);
    return ret;
}

static inline uint64_t root_mem(uint32_t opcode, ...) {
    va_list args;
    va_start(args, opcode);
    uint64_t ret = ERR_UNKNOWN_OPCODE;

    switch (opcode) {
        case MEM_SBRK: {
            int64_t inc = va_arg(args, int64_t);
            ret = syscall_2(MEM, MEM_SBRK, (uint64_t)inc);
            break;
        }
        case MEM_MAP: {
            uint64_t virt  = va_arg(args, uint64_t);
            uint64_t size  = va_arg(args, uint64_t);
            uint64_t flags = va_arg(args, uint64_t);
            ret = syscall_4(MEM, MEM_MAP, virt, size, flags);
            break;
        }
        default:
            break;
    }

    va_end(args);
    return ret;
}

static inline uint64_t root_ipc(uint32_t opcode, ...) {
    va_list args;
    va_start(args, opcode);
    uint64_t ret = ERR_UNKNOWN_OPCODE;

    switch (opcode) {
        case IPC_SEND: {
            const void *buf = va_arg(args, const void *);
            uint64_t len = va_arg(args, uint64_t);
            ret = syscall_3(IPC, IPC_SEND, (uint64_t)buf, len);
            break;
        }
        case IPC_RECV: {
            void *buf = va_arg(args, void *);
            uint64_t max_len = va_arg(args, uint64_t);
            ret = syscall_3(IPC, IPC_RECV, (uint64_t)buf, max_len);
            break;
        }
        default:
            break;
    }

    va_end(args);
    return ret;
}

static inline uint64_t root_proc(uint32_t opcode, ...) {
    va_list args;
    va_start(args, opcode);
    uint64_t ret = ERR_UNKNOWN_OPCODE;

    switch (opcode) {
        case PROC_RUN: {
            const char *filename = va_arg(args, const char *);
            uint64_t file_size = va_arg(args, uint64_t);
            int argc = va_arg(args, int);
            char** argv = va_arg(args, char**);
            ret = syscall_5(PROC, PROC_RUN, (uint64_t)filename, file_size, (uint64_t)argc, (uint64_t)argv);
            break;
        }
        case PROC_EXIT: {
            ret = syscall_1(PROC, PROC_EXIT);
            break;
        }
        default:
            break;
    }

    va_end(args);
    return ret;
}

#endif