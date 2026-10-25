#include "../library/unistd.h"
#include "../library/dogeio.h"

void _start(void) {
    int file = open("test.thing", O_CREAT | O_WRONLY);
    if (file >= 0) {
        char data[] = "thing";
        write(file, data, 35);
        close(file);
    }
    sys_exit(0);
}