#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <basicutil.h>
#include <dogeio.h>
#include <string.h>
#include <boot/syscall_internal.h>
#include <boot/limine.h>

static uint32_t next_pid = 1;

extern volatile struct limine_hhdm_request hhdm_request;

#define USER_CODE_BASE          0x0000000000400000ULL
#define USER_STACK_BASE         0x00007FFFF0000000ULL
#define PAGE_SIZE               4096
#define USER_STACK_PAGE_COUNT   8
#define USER_STACK_SIZE         (PAGE_SIZE * USER_STACK_PAGE_COUNT)
#define USER_ARGUMENT_AREA_SIZE (USER_STACK_SIZE / 2)
#define DATA_SECTION_SIZE       (8 * 1024)

static size_t bin_argument_length(const char *argument) {
    size_t length = 0;
    while (length < 256 && argument[length] != '\0') {
        length++;
    }
    return length;
}

void system_run_bin_impl(char *filename, int program_size) {
    system_run_bin_args_impl(filename, program_size, 0, NULL);
}

static int prepare_user_arguments(uint8_t *stack_memory, int argc, char **argv,
                                uint64_t *user_argv) {
    uint64_t argument_addresses[32];
    size_t cursor = 0;

    if (argc < 0 || argc > 32 || (argc > 0 && argv == NULL)) {
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        if (argv[i] == NULL) {
            return -1;
        }
        size_t length = bin_argument_length(argv[i]);
        if (length == 256 || cursor > USER_ARGUMENT_AREA_SIZE ||
            length + 1 > USER_ARGUMENT_AREA_SIZE - cursor) {
            return -1;
        }
        argument_addresses[i] = USER_STACK_BASE + cursor;
        for (size_t j = 0; j < length; j++) {
            stack_memory[cursor + j] = (uint8_t)argv[i][j];
        }
        stack_memory[cursor + length] = '\0';
        cursor += length + 1;
    }

    cursor = (cursor + sizeof(uint64_t) - 1U) &
             ~(sizeof(uint64_t) - 1U);
    size_t vector_size = ((size_t)argc + 1U) * sizeof(uint64_t);
    if (cursor > USER_ARGUMENT_AREA_SIZE ||
        vector_size > USER_ARGUMENT_AREA_SIZE - cursor) {
        return -1;
    }
    uint64_t *vector = (uint64_t *)(stack_memory + cursor);
    for (int i = 0; i < argc; i++) {
        vector[i] = argument_addresses[i];
    }
    vector[argc] = 0;
    *user_argv = USER_STACK_BASE + cursor;
    return 0;
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
    if (prepare_user_arguments((uint8_t *)USER_STACK_BASE,
                               argc, argv, &user_argv) != 0) {
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
