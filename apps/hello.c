#include <dogeio.h>

__attribute__((section(".text._start")))
int _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    dogeio_text_color_change(COLOR_BRIGHT_CYAN);
    dogeio_text_println("hello doge");
    dogeio_text_color_change(COLOR_WHITE);

    return 0;
} 