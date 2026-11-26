#include "../dogec/dogeio.h"

void _start(int argc, char** argv) {
    if (argc > 0 && argv && argv[0]) {
        printf("%s\n", argv[0]);
    }
    exit();
}