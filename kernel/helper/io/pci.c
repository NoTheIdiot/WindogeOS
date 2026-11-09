#include <system.h>
#include <string.h>
#include <dogeio.h>

#define PCI_CONFIG_ADDRESS  0xCF8U
#define PCI_CONFIG_DATA     0xCFCU

#ifndef PCI_ECAM_BASE
#define PCI_ECAM_BASE 0x40000000ULL
#endif

static uintptr_t g_ecam_base_phys = 0;
extern int g_pcie_initialized;

typedef struct {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    uint64_t address;
    uint64_t size;
    bool is_mmio;
    bool is_64bit;
} pci_bar_t;

typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    uint64_t base_address;
    uint16_t pci_seg_group;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} __attribute__((packed)) mcfg_alloc_t;

typedef struct {
    acpi_sdt_header_t header;
    uint64_t reserved;
    mcfg_alloc_t entries[];
} __attribute__((packed)) acpi_mcfg_t;

void init_pci_ecam(uintptr_t rsdp_phys_addr) {
    if (!rsdp_phys_addr) return;

    uintptr_t rsdp_virt = 0x6FFF00000000ULL;
    map_kernel_mmio_page(rsdp_virt, rsdp_phys_addr & ~0xFFFULL);
    acpi_rsdp_t *rsdp = (acpi_rsdp_t *)(rsdp_virt + (rsdp_phys_addr & 0xFFFULL));

    uintptr_t sdt_phys = (rsdp->revision >= 2 && rsdp->xsdt_address) ? rsdp->xsdt_address : rsdp->rsdt_address;
    uintptr_t sdt_virt = 0x6FFF00001000ULL;
    map_kernel_mmio_page(sdt_virt, sdt_phys & ~0xFFFULL);
    acpi_sdt_header_t *sdt = (acpi_sdt_header_t *)(sdt_virt + (sdt_phys & 0xFFFULL));

    bool is_xsdt = (rsdp->revision >= 2 && rsdp->xsdt_address);
    size_t entry_size = is_xsdt ? 8 : 4;
    size_t entries = (sdt->length - sizeof(acpi_sdt_header_t)) / entry_size;
    uintptr_t entry_ptr = (uintptr_t)sdt + sizeof(acpi_sdt_header_t);

    for (size_t i = 0; i < entries; i++) {
        uint64_t table_phys = is_xsdt ? *(uint64_t *)(entry_ptr + i * 8) : *(uint32_t *)(entry_ptr + i * 4);
        uintptr_t table_virt = 0x6FFF00002000ULL;
        map_kernel_mmio_page(table_virt, table_phys & ~0xFFFULL);
        acpi_sdt_header_t *header = (acpi_sdt_header_t *)(table_virt + (table_phys & 0xFFFULL));

        if (header->signature[0] == 'M' && header->signature[1] == 'C' &&
            header->signature[2] == 'F' && header->signature[3] == 'G') {
            acpi_mcfg_t *mcfg = (acpi_mcfg_t *)header;
            if ((mcfg->header.length - sizeof(acpi_mcfg_t)) >= sizeof(mcfg_alloc_t)) {
                g_ecam_base_phys = (uintptr_t)mcfg->entries[0].base_address;
            }
            return;
        }
    }
}

uint32_t pci_read_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
#if defined(__x86_64__) || defined(__i386__)
    if (g_pcie_initialized) {
        return pcie_read32(bus, slot, func, offset);
    }

    uint32_t address = (uint32_t)((1U << 31) | 
                       ((uint32_t)bus << 16) | 
                       ((uint32_t)slot << 11) | 
                       ((uint32_t)func << 8) | 
                       ((uint32_t)offset & 0xFCU));

    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);

#elif defined(__aarch64__) || defined(__arm__)
    if (g_pcie_initialized) {
        return pcie_read32(bus, slot, func, offset);
    }

    uintptr_t phys = (uintptr_t)PCI_ECAM_BASE + (((uintptr_t)bus << 20) | ((uintptr_t)slot << 15) | ((uintptr_t)func << 12));
    uintptr_t virt = 0x6FFF00003000ULL;
    map_kernel_mmio_page(virt, phys);
    return mmio_read32(virt + (offset & 0xFFCU));
#endif
}

void pci_write_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t val) {
#if defined(__x86_64__) || defined(__i386__)
    if (g_pcie_initialized) {
        pcie_write32(bus, slot, func, offset, val);
        return;
    }

    uint32_t address = (uint32_t)((1U << 31) | 
                       ((uint32_t)bus << 16) | 
                       ((uint32_t)slot << 11) | 
                       ((uint32_t)func << 8) | 
                       ((uint32_t)offset & 0xFCU));

    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);

#elif defined(__aarch64__) || defined(__arm__)
    if (g_pcie_initialized) {
        pcie_write32(bus, slot, func, offset, val);
        return;
    }

    uintptr_t phys = (uintptr_t)PCI_ECAM_BASE + (((uintptr_t)bus << 20) | ((uintptr_t)slot << 15) | ((uintptr_t)func << 12));
    uintptr_t virt = 0x6FFF00003000ULL;
    map_kernel_mmio_page(virt, phys);
    mmio_write32(virt + (offset & 0xFFCU), val);
#endif
}


uint16_t pci_read_16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    uint32_t val = pci_read_32(bus, slot, func, offset);
    return (uint16_t)((val >> (((uint32_t)offset & 2U) * 8U)) & 0xFFFFU);
}

uint8_t pci_read_8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    uint32_t val = pci_read_32(bus, slot, func, offset);
    return (uint8_t)((val >> (((uint32_t)offset & 3U) * 8U)) & 0xFFU);
}

#define PCI_COMMAND_OFFSET 0x04U
#define PCI_CMD_IO_ENABLE  (1U << 0)
#define PCI_CMD_MEM_ENABLE (1U << 1)
#define PCI_CMD_BUS_MASTER (1U << 2)

void pci_enable_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t cmd = pci_read_32(bus, slot, func, PCI_COMMAND_OFFSET);
    cmd |= PCI_CMD_IO_ENABLE | PCI_CMD_MEM_ENABLE | PCI_CMD_BUS_MASTER;
    pci_write_32(bus, slot, func, PCI_COMMAND_OFFSET, cmd);
}

pci_bar_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index) {
    pci_bar_t bar = {0};
    if (bar_index > 5) return bar;

    uint16_t offset = (uint16_t)(0x10U + ((uint16_t)bar_index * 4U));
    uint32_t bar_low = pci_read_32(bus, slot, func, offset);
    if (bar_low == 0U) return bar;

    if (bar_low & 0x1U) {
        bar.is_mmio = false;
        bar.address = (uint64_t)(bar_low & ~0x3U);
    } else {
        bar.is_mmio = true;
        uint8_t type = (uint8_t)((bar_low >> 1) & 0x3U);

        if (type == 0x2U) { 
            bar.is_64bit = true;
            uint32_t bar_high = pci_read_32(bus, slot, func, (uint16_t)(offset + 4U));
            bar.address = ((uint64_t)bar_high << 32) | (uint64_t)(bar_low & ~0xFU);
        } else {
            bar.address = (uint64_t)(bar_low & ~0xFU);
        }
    }

    pci_write_32(bus, slot, func, offset, 0xFFFFFFFFU);
    uint32_t size_mask = pci_read_32(bus, slot, func, offset);
    pci_write_32(bus, slot, func, offset, bar_low);

    if (bar.is_mmio) {
        bar.size = (uint64_t)(~(size_mask & ~0xFU) + 1U);
    } else {
        bar.size = (uint64_t)(~(size_mask & ~0x3U) + 1U);
    }

    return bar;
}

bool pci_find_ahci(uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func, uint64_t *out_bar5_phys) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor0 = pci_read_16((uint8_t)bus, slot, 0, 0x00);
            if (vendor0 == 0xFFFFU) continue;

            uint8_t header = pci_read_8((uint8_t)bus, slot, 0, 0x0E);
            uint8_t max_func = (header & 0x80U) ? 8 : 1;

            for (uint8_t func = 0; func < max_func; func++) {
                uint16_t vendor = pci_read_16((uint8_t)bus, slot, func, 0x00);
                if (vendor == 0xFFFFU) continue;

                uint8_t class_code = pci_read_8((uint8_t)bus, slot, func, 0x0B);
                uint8_t subclass   = pci_read_8((uint8_t)bus, slot, func, 0x0A);

                if (class_code == 0x01U && subclass == 0x06U) {
                    pci_enable_device((uint8_t)bus, slot, func);
                    pci_bar_t bar5 = pci_get_bar((uint8_t)bus, slot, func, 5);
                    if (out_bus) *out_bus = (uint8_t)bus;
                    if (out_slot) *out_slot = slot;
                    if (out_func) *out_func = func;
                    if (out_bar5_phys) *out_bar5_phys = bar5.address;
                    return true;
                }
            }
        }
    }
    return false;
}

char* pci_class_to_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "Unclassified Device (unknown device type)";
        case 0x01: return "Mass Storage Controller (IDE/SATA)";
        case 0x02: return "Network Controller (Ethernet/Wi-Fi)";
        case 0x03: return "Display Controller (VGA)";
        case 0x04: return "Multimedia Controller (Audio)";
        case 0x05: return "Memory Controller";
        case 0x06: return "Bridge Device (Host/ISA)";
        case 0x07: return "Simple Communication Controller";
        case 0x08: return "Base System Peripheral";
        case 0x09: return "Input Device (much keyboard, or mouse)";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor (another CPU?)";
        case 0x0C: return "Serial Bus Controller (USB/SMBus)";
        case 0x0D: return "Wireless Controller";
        default:   return "Unknown Device Type (wtf is this)";
    }
}

bool parse_slot_string(const char* str, uint8_t* bus, uint8_t* slot, uint8_t* func) {
    uint32_t b = 0, s = 0, f = 0;
    const char* p = str;

    while (*p >= '0' && *p <= '9') b = (b * 10) + (uint32_t)(*p++ - '0');
    if (*p != ':') return false; p++;
    while (*p >= '0' && *p <= '9') s = (s * 10) + (uint32_t)(*p++ - '0');
    if (*p != '.') return false; p++;
    while (*p >= '0' && *p <= '9') f = (f * 10) + (uint32_t)(*p++ - '0');

    if (b < 256 && s < 32 && f < 8) {
        *bus = (uint8_t)b; *slot = (uint8_t)s; *func = (uint8_t)f;
        return true;
    }
    return false;
}
