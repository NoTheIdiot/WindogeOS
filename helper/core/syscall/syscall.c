#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <stdint.h>

struct cpu_regs {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15, user_rsp;
};

uint64_t syscall_handler(struct cpu_regs *regs) {
    uint64_t sc_num = regs->rax;
    uint64_t ret_val = 0;

    switch (sc_num) {
        case FS_READ: {
            char* filepath = (char*)regs->rdi;
            char* buffer   = (char*)regs->rsi;
            uint32_t size  = (uint32_t)regs->rdx;

            ret_val = (uint64_t)(int64_t)fs_read(filepath, buffer, size);
            break;
        }

        case FS_WRITE: {
            char* filepath = (char*)regs->rdi;
            char* buffer   = (char*)regs->rsi;

            ret_val = (uint64_t)(int64_t)fs_write(filepath, buffer);
            break;
        }
        
        default:
            ret_val = (uint64_t)-1; 
            break;
    }

    return ret_val;
}