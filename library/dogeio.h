#ifndef DOGEIO_H
#define DOGEIO_H

#include "stdint.h"
#include "root/syscalls.h"

uint64_t test(void) {
    return syscall_0(Print);
}

void exit(void) {
    syscall_0(Process_Exit);
}

#endif