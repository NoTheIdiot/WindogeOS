#include <stdint.h>
#include <stddef.h>

struct limine_acpi_response {
    uint64_t revision;
    uint64_t rsdp;
};

struct limine_acpi_request {
    uint64_t id[4];
    uint64_t revision;
    volatile struct limine_acpi_response *response;
};

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    volatile struct limine_hhdm_response *response;
};

static volatile struct limine_acpi_request acpi_request = {
    .id = { 0xc7b1dd30df4c8b88, 0x0a82e883a0fe68b1, 0x6a40d58844ec8225, 0x1f0d3a58e4ec2b2a },
    .revision = 0
};

static volatile struct limine_hhdm_request hhdm_request = {
    .id = { 0xc7b1dd30df4c8b88, 0x0a82e883a0fe68b1, 0x48d12d30e4f111d3, 0xbc220080c73c8881 },
    .revision = 0
};

static uint64_t g_pcie_base_virtual_addr = 0;
static uint8_t  g_pcie_start_bus = 0;
static uint8_t  g_pcie_end_bus   = 0;
static int      g_pcie_initialized = 0;

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
    uint64_t reg_offset_masked = (uint64_t)(reg_offset & 0xFFF);
    uint64_t total_offset = bus_offset | dev_offset | func_offset | reg_offset_masked;
    
    volatile uint32_t* memory_pointer = (volatile uint32_t*)(g_pcie_base_virtual_addr + total_offset);
    return *memory_pointer;
}

void pcie_write32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset, uint32_t value) {
    if (!g_pcie_initialized) return;
    if (bus < g_pcie_start_bus || bus > g_pcie_end_bus) return;
    if (device >= 32 || function >= 8 || reg_offset >= 4096) return;

    uint64_t total_offset = ((uint64_t)(bus - g_pcie_start_bus) << 20) |
                            ((uint64_t)device << 15) |
                            ((uint64_t)function << 12) |
                            (reg_offset & 0xFFF);

    volatile uint32_t* memory_pointer = (volatile uint32_t*)(g_pcie_base_virtual_addr + total_offset);
    *memory_pointer = value;
}

void init_pcie(void) {
    if (acpi_request.response == NULL || acpi_request.response->rsdp == 0) return;
    if (hhdm_request.response == NULL) return;

    uint8_t* rsdp = (uint8_t*)acpi_request.response->rsdp;
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
        uint64_t virt_base = phys_base + hhdm_offset;

        pcie_init_system(virt_base, start_bus, end_bus);
    }
}
