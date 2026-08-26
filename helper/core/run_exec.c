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

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE   (1ULL << 1)
#define PAGE_USER    (1ULL << 2)

extern volatile struct limine_hhdm_request hhdm_request;

int absoulute = 0;

static uint64_t virt_to_phys(void *ptr, uint64_t hhdm) {
    uint64_t virt = (uint64_t)ptr;
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t *pml4 = (uint64_t *)((cr3 & 0x000FFFFFFFFFF000ULL) + hhdm);
    uint64_t pdpt_e = pml4[(virt >> 39) & 0x1FF];
    if (!(pdpt_e & 1)) return 0;
    if (pdpt_e & 0x80) return (pdpt_e & 0x000FFFFC00000000ULL) | (virt & 0x3FFFFFFF);

    uint64_t *pdpt = (uint64_t *)((pdpt_e & 0x000FFFFFFFFFF000ULL) + hhdm);
    uint64_t pd_e = pdpt[(virt >> 30) & 0x1FF];
    if (!(pd_e & 1)) return 0;
    if (pd_e & 0x80) return (pd_e & 0x000FFFFFFFFFF0000ULL) | (virt & 0x1FFFFF);

    uint64_t *pd = (uint64_t *)((pd_e & 0x000FFFFFFFFFF000ULL) + hhdm);
    uint64_t pt_e = pd[(virt >> 21) & 0x1FF];
    if (!(pt_e & 1)) return 0;

    uint64_t *pt = (uint64_t *)((pt_e & 0x000FFFFFFFFFF000ULL) + hhdm);
    uint64_t pte = pt[(virt >> 12) & 0x1FF];
    if (!(pte & 1)) return 0;

    return (pte & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

static uint64_t map_app_memory(uint64_t stack_phys) {
    if (!hhdm_request.response) return 0;
    uint64_t hhdm = hhdm_request.response->offset;

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *boot_pml4 = (uint64_t *)((cr3 & 0x000FFFFFFFFFF000ULL) + hhdm);

    uint64_t *app_pml4 = (uint64_t *)pmm_alloc_block();
    uint64_t *app_pdpt = (uint64_t *)pmm_alloc_block();
    uint64_t *app_pd   = (uint64_t *)pmm_alloc_block();
    uint64_t *app_stack_pdpt = (uint64_t *)pmm_alloc_block();
    uint64_t *app_stack_pd   = (uint64_t *)pmm_alloc_block();
    uint64_t *app_stack_pt   = (uint64_t *)pmm_alloc_block();

    if (!app_pml4 || !app_pdpt || !app_pd || !app_stack_pdpt || !app_stack_pd || !app_stack_pt) return 0;

    memset(app_pml4, 0, 4096);
    memset(app_pdpt, 0, 4096);
    memset(app_pd, 0, 4096);
    memset(app_stack_pdpt, 0, 4096);
    memset(app_stack_pd, 0, 4096);
    memset(app_stack_pt, 0, 4096);

    for (int i = 256; i < 512; i++) {
        app_pml4[i] = boot_pml4[i];
    }

    uint64_t pml4_phys = virt_to_phys(app_pml4, hhdm);
    uint64_t pdpt_phys = virt_to_phys(app_pdpt, hhdm);
    uint64_t pd_phys   = virt_to_phys(app_pd, hhdm);
    
    uint64_t stack_pml4_idx = (USER_STACK_VIRT >> 39) & 0x1FF;
    uint64_t stack_pdpt_idx = (USER_STACK_VIRT >> 30) & 0x1FF;
    uint64_t stack_pd_idx   = (USER_STACK_VIRT >> 21) & 0x1FF;
    uint64_t stack_pt_idx   = (USER_STACK_VIRT >> 12) & 0x1FF;

    uint64_t stack_pdpt_phys = virt_to_phys(app_stack_pdpt, hhdm);
    uint64_t stack_pd_phys   = virt_to_phys(app_stack_pd, hhdm);
    uint64_t stack_pt_phys   = virt_to_phys(app_stack_pt, hhdm);

    app_pml4[0] = pdpt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    app_pdpt[0] = pd_phys   | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    app_pd[2]   = APP_PHYS_ADDR | (1ULL << 7) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    app_pml4[stack_pml4_idx] = stack_pdpt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    app_stack_pdpt[stack_pdpt_idx] = stack_pd_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    app_stack_pd[stack_pd_idx] = stack_pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    app_stack_pt[stack_pt_idx] = stack_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    return pml4_phys;
}

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

    void *stack_phys_ptr = (void*)pmm_alloc_block();
    if (!stack_phys_ptr) return -4;
    uint64_t stack_phys = virt_to_phys(stack_phys_ptr, hhdm_request.response->offset);

    uint64_t app_pml4_phys = map_app_memory(stack_phys);
    if (!app_pml4_phys) return -5;

    uint8_t *app_write_buf = (uint8_t *)(APP_PHYS_ADDR + hhdm_request.response->offset);
    memset(app_write_buf, 0, MAX_APP_SIZE);

    int bytes_read = fs_read((char *)filename, (char *)app_write_buf, MAX_APP_SIZE);
    if (bytes_read <= 0) {
        return -3;
    }

    uint64_t user_stack_top = USER_STACK_VIRT + 4096 - 16;

    log("Launching binary, just hope.");

    core_to_user(app_pml4_phys, (void *)APP_VIRT_ADDR, (void *)user_stack_top);

    return 0; 
}