#include <dogeio.h>

void dogeio_exit(int code) {
    __asm__ volatile (
        "mov $60, %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "syscall"
        :
        : "r"((uint64_t)code)
        : "rax", "rdi"
    );

    while (1) {}
}

__attribute__((section(".text._start")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    dogeio_text_color_change(COLOR_BRIGHT_CYAN);
    dogeio_text_println("hello doge");
    dogeio_text_color_change(COLOR_WHITE);

    dogeio_exit(0);
} 