#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <basicutil.h>
#include <dogeio.h>
#include <string.h>
#include <boot/syscall_internal.h>
#include <boot/limine.h>
#include "arguments.h"

static uint32_t next_pid = 1;

extern volatile struct limine_hhdm_request hhdm_request;

#define USER_CODE_BASE          0x0000000000400000ULL
#define USER_STACK_BASE         0x00007FFFF0000000ULL
#define PAGE_SIZE               4096
#define USER_STACK_PAGE_COUNT   8
#define USER_STACK_SIZE         (PAGE_SIZE * USER_STACK_PAGE_COUNT)
#define USER_ARGUMENT_AREA_SIZE (USER_STACK_SIZE / 2)
#define DATA_SECTION_SIZE       (8 * 1024)

void system_run_bin_impl(char *filename, int program_size) {
    system_run_bin_args_impl(filename, program_size, 0, NULL);
}

void system_run_bin_args_impl(char *filename, int program_size, int argc, char **argv) {
    if (!filename || program_size <= 0) {
        dogeio_println("[Error] Invalid binary filename or size.");
        return;
    }

    if (hhdm_request.response == NULL) {
        dogeio_println("[Error] HHDM response is NULL.");
        return;
    }

    size_t required_code_pages = ((size_t)program_size + PAGE_SIZE - 1) / PAGE_SIZE;

    cleanup_user_pages();
    log("[dogeing] cleaned user pages");

    uint64_t hhdm_offset = hhdm_request.response->offset;
    size_t loaded_bytes = 0;

    for (size_t i = 0; i < required_code_pages; i++) {
        uint64_t offset = i * PAGE_SIZE;

        uint64_t code_phys = pmm_alloc_zeroed_page();
        if (!code_phys) {
            uint32_t saved = dogeio_color;
            dogeio_color_change(COLOR_RED);
            dogeio_println("[Error] Out of physical RAM while loading binary.");
            cleanup_user_pages();
            dogeio_color_change(saved);
            return;
        }

        size_t bytes_to_read = (size_t)program_size - offset;
        if (bytes_to_read > PAGE_SIZE) {
            bytes_to_read = PAGE_SIZE;
        }

        uint8_t *page_dst = (uint8_t *)(code_phys + hhdm_offset);
        map_user_page(USER_CODE_BASE + offset, code_phys);

        int read_bytes = fs_read_raw_at(filename, page_dst, offset, bytes_to_read);
        if (read_bytes < 0 || (size_t)read_bytes > bytes_to_read) {
            dogeio_color_change(COLOR_RED);
            uint32_t saved = dogeio_color;
            dogeio_println("[Error] Failed to read binary from exFAT filesystem.");
            cleanup_user_pages();
            dogeio_color_change(saved);
            return;
        }

        if (read_bytes == 0) {
            break;
        }

        loaded_bytes += (size_t)read_bytes;

        if ((size_t)read_bytes < bytes_to_read) {
            break;
        }
    }

    if (loaded_bytes == 0) {
        dogeio_println("[Error] Binary file is empty or could not be read.");
        cleanup_user_pages();
        return;
    }

    serial_print("[dogeing] mapping user stack pages\n");
    for (size_t i = 0; i < USER_STACK_PAGE_COUNT; i++) {
        uint64_t stack_phys = pmm_alloc_zeroed_page();
        if (!stack_phys) {
            uint32_t saved = dogeio_color;
            dogeio_color_change(COLOR_RED);
            dogeio_println("[Error] Out of physical RAM for user stack.");
            cleanup_user_pages();
            dogeio_color_change(saved);
            return;
        }
        map_user_page(USER_STACK_BASE + i * PAGE_SIZE, stack_phys);
    }

    uint64_t user_argv = 0;
    if (prepare_user_arguments((uint8_t *)USER_STACK_BASE, USER_STACK_BASE,
                               USER_ARGUMENT_AREA_SIZE, argc, argv,
                               &user_argv) != 0) {
        dogeio_println("[Error] Application arguments exceed the user stack.");
        cleanup_user_pages();
        return;
    }

    uint64_t user_stack_top = (USER_STACK_BASE + USER_STACK_SIZE) - 8;

    log("[dogeing] executing binary");

    to_userland_ring3_args(USER_CODE_BASE, user_stack_top, (uint64_t)argc,
                           user_argv);
}

process_t* spawn_process(void (*entry_point)(void), uint64_t user_stack_top, uint64_t page_dir) {
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) return NULL;

    size_t kstack_size = 8192;
    uint8_t* kstack_base = (uint8_t*)kmalloc(kstack_size);
    if (!kstack_base) return NULL;

    uint8_t* kstack_top = kstack_base + kstack_size;
    syscall_registers_t* frame = (syscall_registers_t*)(kstack_top - sizeof(syscall_registers_t));
    
    uint64_t* raw_frame = (uint64_t*)frame;
    for (size_t i = 0; i < sizeof(syscall_registers_t) / 8; i++) {
        raw_frame[i] = 0;
    }

    frame->rcx = (uint64_t)entry_point;
    frame->r11 = 0x202;
    frame->user_rsp = user_stack_top;
    frame->rax = 0xDEADC0DE;
    frame->rbx = 0x11111111;

    proc->kernel_rsp = (uint64_t*)frame;
    proc->pid = next_pid++;
    proc->status = 1;
    proc->page_directory = page_dir;
    proc->next = NULL;

    return proc;
}
