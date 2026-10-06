#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <basicutil.h>
#include <dogeio.h>
#include <string.h>
#include <boot/limine.h>

extern volatile struct limine_hhdm_request hhdm_request;
#define USER_CODE_BASE  0x0000000000400000ULL
#define USER_STACK_BASE 0x00007FFFF0000000ULL
#define PAGE_SIZE       4096
#define MAX_FLAT_BINARY_SIZE (16 * 1024 * 1024)

void system_run_bin_impl(char *filename, int program_size) {
    if (!filename || program_size <= 0 || program_size > MAX_FLAT_BINARY_SIZE) {
        dogeio_text_println("[Error] Invalid binary filename or size.");
        return;
    }

    if (hhdm_request.response == NULL) {
        dogeio_text_println("[Error] HHDM response is NULL.");
        return;
    }

    cleanup_user_pages();
    log("[dogeing] cleaned user pages");

    uint64_t hhdm_offset = hhdm_request.response->offset;

    size_t required_pages = ((size_t)program_size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t loaded_bytes = 0;

    for (size_t i = 0; i < required_pages; i++) {
        uint64_t offset = i * PAGE_SIZE;

        uint64_t code_phys = pmm_alloc_zeroed_page();
        char code_phys_str[32];
        uint64_to_str(code_phys, code_phys_str);
        char* actual_string_ptr = uint64_to_str(code_phys, code_phys_str);
        serial_print("[dogeing] mapped page ");
        log(actual_string_ptr);
        if (!code_phys) {
            log("[Error] Out of physical memory loading binary.");
            return;
        }

        size_t bytes_to_read = (size_t)program_size - offset;
        if (bytes_to_read > PAGE_SIZE) {
            bytes_to_read = PAGE_SIZE;
        }

        uint8_t *page_dst = (uint8_t *)(code_phys + hhdm_offset);

        int read_bytes = fs_read_raw_at(filename, page_dst, offset, bytes_to_read);
        if (read_bytes < 0 || (size_t)read_bytes > bytes_to_read) {
            log("[Error] Failed to read binary from exFAT filesystem.");
            cleanup_user_pages();
            return;
        }

        if (read_bytes == 0) {
            break;
        }

        map_user_page(USER_CODE_BASE + offset, code_phys);
        loaded_bytes += (size_t)read_bytes;

        if ((size_t)read_bytes < bytes_to_read) {
            break;
        }
    }

    if (loaded_bytes == 0) {
        log("[Error] Binary file is empty or could not be read.");
        cleanup_user_pages();
        return;
    }

    uint64_t stack_phys = pmm_alloc_zeroed_page();
    char stack_phys_str[32];
    char* actual_stack_ptr = uint64_to_str(stack_phys, stack_phys_str);
    
    serial_print("[dogeing] page mapped for stack ");
    log(actual_stack_ptr);

    if (!stack_phys) {
        log("[Error] Out of physical memory for user stack.");
        cleanup_user_pages();
        return;
    }
    map_user_page(USER_STACK_BASE, stack_phys);

    uint64_t user_stack_top = (USER_STACK_BASE + PAGE_SIZE) - 8;

    log("[dogeing] executing binary");

    to_userland_ring3(USER_CODE_BASE, user_stack_top);
}
