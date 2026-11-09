#include <system.h>
#include <string.h>
#include <dogeio.h>
#include <stdint.h>
#include <stddef.h>
#include <syscalls.h>

uint64_t syscall_handler_switch(registers_t regs, uint64_t sc_num) {
    switch (sc_num) {
        case Process_Exit: {
            asm volatile ("sysretq");
            return 0;
        }

        case Print: {
            print("Programs work\n");
            return 0;
        }

        default:
            println("[\033[91mNot Wow\033[0m] Unknown system call tried to execute, but doesn't have id.");
            break;
    }
}