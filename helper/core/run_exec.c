#include <system.h>
#include <stdint.h>
#include <stddef.h>
#include <basicutil.h>
#include <dogeio.h>
#include <boot/limine.h>

extern volatile struct limine_hhdm_request hhdm_request;
extern uint64_t kernel_program_launcher_rsp;

#define USER_CODE_BASE  0x0000000000400000ULL
#define USER_STACK_BASE 0x00007FFFF0000000ULL
#define PAGE_SIZE       4096

void cleanup_user_pages(void) {
    if (hhdm_request.response == NULL) {
        return;
    }

    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint64_t *pml4 = (uint64_t *)((read_cr3() & ~0xFFFULL) + hhdm_offset);

    for (int i = 0; i < 256; i++) {
        pml4[i] = 0;
    }

    write_cr3(read_cr3());
}

void system_run_exec(char *filename, int program_size) {
    if (!filename || program_size <= 0) {
        dogeio_text_println("[Error] Invalid binary filename or size.");
        return;
    }

    if (hhdm_request.response == NULL) {
        dogeio_text_println("[Error] HHDM response is NULL.");
        return;
    }

    cleanup_user_pages();

    uint64_t hhdm_offset = hhdm_request.response->offset;

    size_t required_pages = ((size_t)program_size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < required_pages; i++) {
        uint64_t offset = i * PAGE_SIZE;

        uint64_t code_phys = pmm_alloc_zeroed_page();
        if (!code_phys) {
            dogeio_text_println("[Error] Out of physical memory loading binary.");
            return;
        }

        size_t bytes_to_read = (size_t)program_size - offset;
        if (bytes_to_read > PAGE_SIZE) {
            bytes_to_read = PAGE_SIZE;
        }

        uint8_t *page_dst = (uint8_t *)(code_phys + hhdm_offset);

        int read_bytes = fs_read_raw(filename, page_dst, (uint32_t)bytes_to_read);
        if (read_bytes < 0) {
            dogeio_text_println("[Error] Failed to read binary from exFAT filesystem.");
            return;
        }

        map_user_page(USER_CODE_BASE + offset, code_phys);
    }

    uint64_t stack_phys = pmm_alloc_zeroed_page();
    if (!stack_phys) {
        dogeio_text_println("[Error] Out of physical memory for user stack.");
        return;
    }
    map_user_page(USER_STACK_BASE, stack_phys);

    uint64_t user_stack_top = (USER_STACK_BASE + PAGE_SIZE) - 8;

    dogeio_text_println("[Kernel] Dropping to Ring 3 execution...");

    __asm__ volatile("mov %%rsp, %0" : "=m"(kernel_program_launcher_rsp));

    to_userland_ring3(USER_CODE_BASE, user_stack_top);
}
