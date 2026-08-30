#include <stdint.h>
#include <string.h>
#include <core.h>
#include <dogeio.h>
#include <basicutil.h>
#include <boot/limine.h>

#define APP_PHYS_ADDR 0x01000000ULL
#define APP_VIRT_ADDR 0x00400000ULL
#define USER_STACK_VIRT 0x7FFF00000000ULL
#define MAX_APP_SIZE  (2 * 1024 * 1024)

extern uint64_t create_user_pml4(void);
extern void map_user_page(uint64_t pml4_phys, uint64_t virtual_addr, uint64_t physical_addr);
extern uint64_t allocate_flat_user_stack(uint64_t hhdm, uint64_t pml4_phys);

extern volatile struct limine_hhdm_request hhdm_request;
int absoulute = 0;

int exec_flat_binary(const char *filename, int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (!absoulute) {
        init_user_space();
        absoulute = 1;
    }

    if (!fs_exists((char *)filename)) {
        dogeio_text_println("Error: Binary file not found.");
        return -1;
    }

    if (!hhdm_request.response) {
        return -2;
    }
    uint64_t hhdm = hhdm_request.response->offset;

    uint64_t app_pml4_phys = create_user_pml4();
    if (!app_pml4_phys) return -5;

    map_user_page(app_pml4_phys, APP_VIRT_ADDR, APP_PHYS_ADDR);

    if (!allocate_flat_user_stack(hhdm, app_pml4_phys)) {
        return -4;
    }

    uint8_t *app_write_buf = (uint8_t *)(APP_PHYS_ADDR + hhdm);
    memset(app_write_buf, 0, MAX_APP_SIZE);

    int bytes_read = fs_read((char *)filename, (char *)app_write_buf, MAX_APP_SIZE);
    if (bytes_read <= 0) {
        return -3;
    }

    uint64_t user_stack_top = USER_STACK_VIRT - 16;

    log("Launching binary, just hope.");

    core_to_user(app_pml4_phys, (void *)APP_VIRT_ADDR, (void *)user_stack_top);

    return 0; 
}
