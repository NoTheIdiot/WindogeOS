#ifndef DOGELIB_H
#define DOGELIB_H

#include "root/syscalls.h"

static inline int windoge_shutdown(void) {
    return (int)syscall_0(Shutdown);
}

static inline int windoge_reboot(void) {
    return (int)syscall_0(Reboot);
}

static inline int beep(uint32_t hz, uint32_t time) {
    return (int)syscall_2(Beep, (uint64_t)hz, (uint64_t)time);
}

static inline int sleep(uint32_t hz) {
    return (int)syscall_1(Sleep, (uint64_t)hz);
}

#endif