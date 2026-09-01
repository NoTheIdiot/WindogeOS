#include <user/dogeio.h>

void _start(void) {
    dogeio_text_print("hello from ring 3\n");
    dogeio_update();
    dogeio_exit();

    for (;;) {
    }
}