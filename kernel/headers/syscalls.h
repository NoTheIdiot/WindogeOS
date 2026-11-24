#ifndef SYSCALLS_H
#define SYSCALLS_H

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
    DOGE_RAND     = 0x06
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
    IPC_SEND = 1,
    IPC_RECV = 2
};

enum {
    PROC_EXEC = 1,
    PROC_EXIT = 2
};

#define ERR_UNKNOWN_SYSCALL ((uint64_t)-1)
#define ERR_UNKNOWN_OPCODE  ((uint64_t)-2)
#define ERR_BAD_PTR         ((uint64_t)-3)
#define ERR_INVAL           ((uint64_t)-4)

#endif