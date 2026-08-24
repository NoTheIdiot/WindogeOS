#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stddef.h>

#define DOGEIO_TEXT_PUTCHAR           1
#define DOGEIO_TEXT_CLEAR             2
#define DOGEIO_TEXT_PRINTCHAR         3
#define DOGEIO_TEXT_PRINT             4
#define DOGEIO_TEXT_PRINTLN           5
#define DOGEIO_TEXT_PRINT_AT          6
#define DOGEIO_TEXT_INPUT             7
#define DOGEIO_TEXT_COLOR_CHANGE      8
#define DOGEIO_TEXT_BACKGROUND_CHANGE 9
#define DOGEIO_TEXT_CLEAR_RAW         10

#define DOGEIO_FS_FORMAT              11
#define DOGEIO_FS_CREATE              12
#define DOGEIO_FS_MKDIR               13
#define DOGEIO_FS_EXISTS              14
#define DOGEIO_FS_DELETE              15
#define DOGEIO_FS_DELETE_LAST_LINE    16
#define DOGEIO_FS_READ                17
#define DOGEIO_FS_WRITE               18
#define DOGEIO_FS_LIST_DIR            19
#define DOGEIO_FS_RENAME              20
#define DOGEIO_FS_COPY                21
#define DOGEIO_FS_CHDIR               22
#define DOGEIO_FS_DIRNAME             23
#define DOGEIO_FS_MOUNT               24

#define DOGEIO_EXEC_FLAT_BINARY       25

extern void *kernel_stack_top;
extern void *user_rsp_scratch;

void syscall_entry(void);
uint64_t c_syscall_handler(uint64_t sys_id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);
void core_to_user(void *user_entry, void *user_stack_top);

static inline uint64_t core_syscall(uint64_t sys_id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    uint64_t result;
    register uint64_t r10 __asm__("r10") = arg4;

    __asm__ volatile (
        "syscall\n\t"
        : "=a"(result)
        : "a"(sys_id), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );

    return result;
}

#endif