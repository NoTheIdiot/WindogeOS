#include <dogeio.h>
#include <stdint.h>
#include <string.h>
#include <basicutil.h>
#include <boot/limine.h>

#define APP_PHYS_ADDR 0x00200000
#define MAX_APP_SIZE  (2 * 1024 * 1024)

extern volatile struct limine_hhdm_request hhdm_request;

typedef int (*app_entry_t)(int argc, char **argv);

static uint64_t app_pdpt[512] __attribute__((aligned(4096)));
static uint64_t app_pd[512]   __attribute__((aligned(4096)));

static uint64_t virt_to_phys(void *ptr, uint64_t hhdm) {
    uint64_t virt = (uint64_t)ptr;
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t *pml4 = (uint64_t *)((cr3 & 0x000FFFFFFFFFF000ULL) + hhdm);
    uint64_t pdpt_e = pml4[(virt >> 39) & 0x1FF];
    if (!(pdpt_e & 1)) return 0;

    uint64_t *pdpt = (uint64_t *)((pdpt_e & 0x000FFFFFFFFFF000ULL) + hhdm);
    uint64_t pd_e = pdpt[(virt >> 30) & 0x1FF];
    if (!(pd_e & 1)) return 0;
    if (pd_e & 0x80) return (pd_e & 0x000FFFFFFFFF0000ULL) | (virt & 0x3FFFFFFF);

    uint64_t *pd = (uint64_t *)((pd_e & 0x000FFFFFFFFFF000ULL) + hhdm);
    uint64_t pt_e = pd[(virt >> 21) & 0x1FF];
    if (!(pt_e & 1)) return 0;
    if (pt_e & 0x80) return (pt_e & 0x000FFFFFFFE00000ULL) | (virt & 0x1FFFFF);

    uint64_t *pt = (uint64_t *)((pt_e & 0x000FFFFFFFFFF000ULL) + hhdm);
    uint64_t pte = pt[(virt >> 12) & 0x1FF];
    if (!(pte & 1)) return 0;

    return (pte & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

static void map_app_memory(void) {
    if (!hhdm_request.response) return;

    uint64_t hhdm = hhdm_request.response->offset;

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t *pml4 = (uint64_t *)((cr3 & 0x000FFFFFFFFFF000ULL) + hhdm);

    uint64_t pdpt_phys = virt_to_phys(app_pdpt, hhdm);
    uint64_t pd_phys   = virt_to_phys(app_pd, hhdm);

    memset(app_pdpt, 0, 4096);
    memset(app_pd, 0, 4096);

    pml4[0] = pdpt_phys | 0x07;
    app_pdpt[0] = pd_phys | 0x07;
    app_pd[1] = APP_PHYS_ADDR | 0x87;

    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

int exec_flat_binary(const char *filename, int argc, char **argv) {
    if (!fs_exists((char *)filename)) {
        dogeio_text_println("Error: Binary file not found.");
        return -1;
    }

    if (!hhdm_request.response) {
        log("Error: HHDM missing.");
        return -2;
    }

    map_app_memory();

    uint8_t *app_write_buf = (uint8_t *)(APP_PHYS_ADDR + hhdm_request.response->offset);
    memset(app_write_buf, 0, MAX_APP_SIZE);

    int bytes_read = fs_read((char *)filename, (char *)app_write_buf, MAX_APP_SIZE);
    if (bytes_read <= 0) {
        log("Failed to read file");
        return -3;
    }

    app_entry_t app_entry = (app_entry_t)APP_PHYS_ADDR;
    return app_entry(argc, argv);
}