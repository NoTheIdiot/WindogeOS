#include "../library/unistd.h"
#include "../library/dogeio.h"

void _start(void) {
    int file = open("test.thing", O_CREAT | O_WRONLY, 0644);
    if (file >= 0) {
        char data[] = "thing";
        write(file, data, sizeof(data) - 1);
        close(file);
    }
    exit(0);
}