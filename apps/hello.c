#include <dogeio.h>

__attribute__((noreturn))
void dogeio_exit(int code) {
    register uint64_t rax __asm__("rax") = 60;
    register uint64_t rdi __asm__("rdi") = (uint64_t)code;

    __asm__ volatile (
        "syscall"
        :
        : "r"(rax), "r"(rdi)
        : "rcx", "r11", "memory"
    );

    while (1) {
        __asm__ volatile ("pause");
    }
}

__attribute__((section(".text._start"), noreturn))
void _start(void) {
    dogeio_text_color_change(COLOR_BRIGHT_CYAN);
    dogeio_text_println("hello doge");
    dogeio_text_color_change(COLOR_WHITE);

    dogeio_exit(0);
}