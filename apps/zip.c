#include "../library/stddef.h"
#include "../library/stdint.h"
#include "../library/dogeio.h"
#include "../library/string.h"

void _start(int argc, char** argv) {
    int status = 0;
    if (argc == 1 && str_strcmp(argv[0], "--help") == 0) {
        println("Usage: zip <file> <output>");
        println("No zip yet, be patient");
        status = 0;
    } else if (argc == 1 && str_strcmp(argv[0], "--please") == 0) {
        println("Be a little patient sir");
        status = 0;
    } else if (argc != 0) {
        println("Usage: zip <file> <output>");
        status = 2;
    }
    sys_exit((uint64_t)status);
}