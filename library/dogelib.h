#ifndef DOGELIB_H
#define DOGELIB_H

#include "root/syscalls.h"

static inline int windoge_shutdown(void) {
    return (int)syscall_0(Shutdown);
}

static inline int windoge_reboot(void) {
    return (int)syscall_0(Reboot);
}


#endif