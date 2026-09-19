// pci stuff
#include <stdint.h>
#include <stddef.h>
#include <system.h>
#include <basicutil.h>

#define PCI_CONFIG_ADDRESS  0xCF8U
#define PCI_CONFIG_DATA     0xCFCU

#ifndef PCI_ECAM_BASE
#define PCI_ECAM_BASE 0x40000000ULL
#endif

uint32_t pci_read_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t address = (uint32_t)((1U << 31) | 
                       ((uint32_t)bus << 16) | 
                       ((uint32_t)slot << 11) | 
                       ((uint32_t)func << 8) | 
                       ((uint32_t)offset & 0xFCU));

    ports_outl(PCI_CONFIG_ADDRESS, address);
    return ports_inl(PCI_CONFIG_DATA);

#elif defined(__aarch64__) || defined(__arm__)
    uintptr_t ecam_addr = (uintptr_t)PCI_ECAM_BASE |
                          ((uintptr_t)bus  << 20) |
                          ((uintptr_t)slot << 15) |
                          ((uintptr_t)func << 12) |
                          ((uintptr_t)offset & 0xFFFU);

    return ports_inl(ecam_addr);
#endif
}

void pci_write_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t val) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t address = (uint32_t)((1U << 31) | 
                       ((uint32_t)bus << 16) | 
                       ((uint32_t)slot << 11) | 
                       ((uint32_t)func << 8) | 
                       ((uint32_t)offset & 0xFCU));

    ports_outl(PCI_CONFIG_ADDRESS, address);
    ports_outl(PCI_CONFIG_DATA, val);

#elif defined(__aarch64__) || defined(__arm__)
    uintptr_t ecam_addr = (uintptr_t)PCI_ECAM_BASE |
                          ((uintptr_t)bus  << 20) |
                          ((uintptr_t)slot << 15) |
                          ((uintptr_t)func << 12) |
                          ((uintptr_t)offset & 0xFFFU);

    ports_outl(ecam_addr, val);
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

// some of the actual stuff
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