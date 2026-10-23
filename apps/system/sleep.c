#include "../library/dogeio.h"
#include "../library/stddef.h"
#include "../library/bool.h"

void _start(void) {
    disable_bar();
    clear_raw();
    while (true) {
        uint16_t key = (uint16_t)get_key_delay(1000);
        __asm__ volatile (
            "mov $100, %%rcx\n\t"
            "1:\n\t"
            "pause\n\t"
            "loop 1b"
            :
            :
            : "rcx"
        );
        if (key != KEY_UNKNOWN) {
            break;
        }
    }
    enable_bar();
    println("");
    sys_exit(0);
}