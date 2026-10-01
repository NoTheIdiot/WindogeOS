#include <user/dogeio.h>

__attribute__((section(".text.entry"))) void _start(void) {
    syscall_0(SPECIAL);
    syscall_1(SYS_EXIT, 0);
    while(1);
}