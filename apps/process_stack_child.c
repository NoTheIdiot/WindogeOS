#include "../library/dogelib.h"
#include "../library/dogeio.h"

static int strings_equal(const char *left, const char *right) {
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

void _start(uint64_t argc, char **argv) {
    if (argc != 1 || argv == NULL) {
        sys_exit(40);
        for (;;) {
            __asm__ volatile ("pause");
        }
    }

    if (strings_equal(argv[0], "inner")) {
        sys_exit(37);
    } else if (strings_equal(argv[0], "outer")) {
        char *child_arguments[] = {"inner"};
        uint64_t child_status = process_stack(
            "/apps/process_stack_child.elf", 1, child_arguments);
        sys_exit(child_status == 37 ? 38 : 39);
    } else {
        sys_exit(40);
    }

    for (;;) {
        __asm__ volatile ("pause");
    }
}
