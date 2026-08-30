#ifndef USER_DOGEIO_H
#define USER_DOGEIO_H

#include <boot/syscall.h>

static inline void dogeio_exit(void) {
    _syscall(0, 0);
}

static inline void dogeio_text_print(const char* text) {
    _syscall(DOGEIO_TEXT_PRINT, (uint64_t)text);
}

#endif