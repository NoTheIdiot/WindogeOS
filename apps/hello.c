#include <user/dogeio.h>

void _start(void) {
    dogeio_update();
    dogeio_text_print("A month wasn't wasted.\n");
    dogeio_update();
    dogeio_exit();

    while(1);
}