#include <dogeio.h>

__attribute__((section(".text._start")))
int _start(void) {
    dogeio_text_color_change(COLOR_BRIGHT_CYAN);
    dogeio_text_println("hello mario");
    dogeio_text_color_change(COLOR_WHITE);
    return 0;
}