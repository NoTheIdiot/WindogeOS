#include <stdint.h>
#include <system.h>
#include <dogeio.h>

#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <system.h>

extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_hhdm_request hhdm_request;

static uint64_t g_pcie_base_virtual_addr = 0;
static uint8_t  g_pcie_start_bus = 0;
static uint8_t  g_pcie_end_bus   = 0;
int      g_pcie_initialized = 0;

void pcie_init_system(uint64_t v_addr, uint8_t start_bus, uint8_t end_bus) {
    g_pcie_base_virtual_addr = v_addr;
    g_pcie_start_bus = start_bus;
    g_pcie_end_bus   = end_bus;
    g_pcie_initialized = 1;
}

uint32_t pcie_read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset) {
    if (!g_pcie_initialized) return 0xFFFFFFFF;
    if (bus < g_pcie_start_bus || bus > g_pcie_end_bus) return 0xFFFFFFFF;
    if (device >= 32 || function >= 8 || reg_offset >= 4096) return 0xFFFFFFFF;

    uint64_t bus_offset  = (uint64_t)(bus - g_pcie_start_bus) << 20;
    uint64_t dev_offset  = (uint64_t)device << 15;
    uint64_t func_offset = (uint64_t)function << 12;
    uint64_t reg_offset_masked = (uint64_t)(reg_offset & 0xFFC);
    uint64_t total_offset = bus_offset | dev_offset | func_offset | reg_offset_masked;

    return mmio_read32((uintptr_t)(g_pcie_base_virtual_addr + total_offset));
}

void pcie_write32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset, uint32_t value) {
    if (!g_pcie_initialized) return;
    if (bus < g_pcie_start_bus || bus > g_pcie_end_bus) return;
    if (device >= 32 || function >= 8 || reg_offset >= 4096) return;

    uint64_t total_offset = ((uint64_t)(bus - g_pcie_start_bus) << 20) |
                            ((uint64_t)device << 15) |
                            ((uint64_t)function << 12) |
                            (reg_offset & 0xFFC);

    mmio_write32((uintptr_t)(g_pcie_base_virtual_addr + total_offset), value);
}

void init_pcie(void) {
    if (rsdp_request.response == NULL || rsdp_request.response->address == NULL) return;
    if (hhdm_request.response == NULL) return;

    uint8_t* rsdp = (uint8_t*)rsdp_request.response->address;
    uint8_t* mcfg_table = NULL;
    uint64_t hhdm_offset = hhdm_request.response->offset;
    uint8_t revision = *(uint8_t*)(rsdp + 15);

    if (revision >= 2) {
        uint64_t xsdt_addr = *(uint64_t*)(rsdp + 24);
        if (xsdt_addr != 0) {
            uint8_t* xsdt = (uint8_t*)(uintptr_t)(xsdt_addr + hhdm_offset);
            uint32_t length = *(uint32_t*)(xsdt + 4);
            int entries = (length - 36) / 8;
            uint64_t* table_pointers = (uint64_t*)(xsdt + 36);

            for (int i = 0; i < entries; i++) {
                uint8_t* table = (uint8_t*)(uintptr_t)(table_pointers[i] + hhdm_offset);
                if (table[0] == 'M' && table[1] == 'C' && table[2] == 'F' && table[3] == 'G') {
                    mcfg_table = table;
                    break;
                }
            }
        }
    } else {
        uint32_t rsdt_addr = *(uint32_t*)(rsdp + 16);
        if (rsdt_addr != 0) {
            uint8_t* rsdt = (uint8_t*)(uintptr_t)(rsdt_addr + hhdm_offset);
            uint32_t length = *(uint32_t*)(rsdt + 4);
            int entries = (length - 36) / 4;
            uint32_t* table_pointers = (uint32_t*)(rsdt + 36);

            for (int i = 0; i < entries; i++) {
                uint8_t* table = (uint8_t*)(uintptr_t)(table_pointers[i] + hhdm_offset);
                if (table[0] == 'M' && table[1] == 'C' && table[2] == 'F' && table[3] == 'G') {
                    mcfg_table = table;
                    break;
                }
            }
        }
    }

    if (mcfg_table != NULL) {
        uint8_t* allocations = mcfg_table + 44;
        uint64_t phys_base = *(uint64_t*)(allocations);
        uint8_t start_bus  = *(uint8_t*)(allocations + 10);
        uint8_t end_bus    = *(uint8_t*)(allocations + 11);

        uint64_t ecam_virt_base = 0x4FFFF0000000ULL;
        uint64_t total_pages = ((uint64_t)(end_bus - start_bus + 1) << 20) / PAGE_SIZE;

        for (uint64_t p = 0; p < total_pages; p++) {
            uint64_t offset = p * PAGE_SIZE;
            map_kernel_mmio_page((uintptr_t)(ecam_virt_base + offset), phys_base + offset);
        }

        pcie_init_system(ecam_virt_base, start_bus, end_bus);
    }
}
