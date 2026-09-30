#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <boot/limine.h>
#include <system.h>

#define NVME_REG_CC_EN          1U
#define NVME_REG_CSTS_RDY       1U
#define NVME_ADMIN_IDENTIFY     0x06
#define QUEUE_SIZE              64

static volatile uint8_t* regs;
static uint8_t* asq_ring;
static uint8_t* acq_ring;

static uint16_t asq_tail = 0;
static uint16_t acq_head = 0;
static uint16_t command_id_counter = 0;

static uint32_t db_stride;
static volatile uint32_t* asq_db;
static volatile uint32_t* acq_db;

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;

static uintptr_t get_hhdm_virtual(uint64_t physical_address) {
    return (uintptr_t)(physical_address + hhdm_request.response->offset);
}

static uint64_t get_physical_from_hhdm(uintptr_t virtual_address) {
    return (uint64_t)(virtual_address - hhdm_request.response->offset);
}

static uint64_t allocate_limine_physical_page(void) {
    struct limine_memmap_response *mmap = memmap_request.response;
    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        struct limine_memmap_entry *entry = mmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= 4096) {
            uint64_t allocated_phys = entry->base;
            entry->base += 4096;
            entry->length -= 4096;
            return allocated_phys;
        }
    }
    return 0; 
}

void nvme_init(uintptr_t bar0_mem_base) {
    regs = (volatile uint8_t*)bar0_mem_base;

    *(volatile uint32_t*)(regs + 0x14) &= ~NVME_REG_CC_EN;
    while (*(volatile uint32_t*)(regs + 0x1C) & NVME_REG_CSTS_RDY);

    uint64_t cap = *(volatile uint64_t*)(regs + 0x00);
    db_stride = (uint32_t)((cap >> 32) & 0xF); 
    
    asq_db = (volatile uint32_t*)(regs + 0x1000);
    acq_db = (volatile uint32_t*)(regs + 0x1000 + (1 << (db_stride + 2)));

    uintptr_t asq_virt = get_hhdm_virtual(allocate_limine_physical_page());
    uintptr_t acq_virt = get_hhdm_virtual(allocate_limine_physical_page());

    asq_ring = (uint8_t*)asq_virt;
    acq_ring = (uint8_t*)acq_virt;
    
    memset(asq_ring, 0, 4096);
    memset(acq_ring, 0, 4096);

    *(volatile uint32_t*)(regs + 0x24) = ((QUEUE_SIZE - 1) << 16) | (QUEUE_SIZE - 1);
    *(volatile uint64_t*)(regs + 0x28) = get_physical_from_hhdm(asq_virt);
    *(volatile uint64_t*)(regs + 0x30) = get_physical_from_hhdm(acq_virt);

    *(volatile uint32_t*)(regs + 0x14) |= NVME_REG_CC_EN;
    while (!(*(volatile uint32_t*)(regs + 0x1C) & NVME_REG_CSTS_RDY));
}

void nvme_submit_admin_cmd(uint8_t* raw_cmd, uint8_t* target_cpl) {
    uint16_t cid = command_id_counter++;
    *(uint16_t*)(raw_cmd + 2) = cid;
    
    memcpy(asq_ring + (asq_tail * 64), raw_cmd, 64);
    
    asq_tail = (asq_tail + 1) % QUEUE_SIZE;
    
    *asq_db = asq_tail;

    volatile uint8_t* cpl = acq_ring + (acq_head * 16);
    while (((*(volatile uint16_t*)(cpl + 14)) & 1) == (acq_head / QUEUE_SIZE) % 2);

    memcpy(target_cpl, (const void*)cpl, 16);

    acq_head = (acq_head + 1) % QUEUE_SIZE;
    
    *acq_db = acq_head;
}

void nvme_identify_drive(uint64_t destination_physical_buffer) {
    uint8_t cmd[64];
    uint8_t cpl[16];
    
    memset(cmd, 0, 64);
    memset(cpl, 0, 16);

    cmd[0] = NVME_ADMIN_IDENTIFY; 
    *(uint32_t*)(cmd + 4) = 0;    
    *(uint64_t*)(cmd + 24) = destination_physical_buffer; 
    *(uint32_t*)(cmd + 40) = 1;   

    nvme_submit_admin_cmd(cmd, cpl);
}

void nvme_pci_callback(uint8_t b, uint8_t d, uint8_t f, uint16_t ven, uint16_t dev_id) {
    (void)ven;
    (void)dev_id;

    uint32_t class_reg = pcie_read32(b, d, f, 0x08);
    uint8_t class_code = (class_reg >> 24) & 0xFF;
    uint8_t subclass   = (class_reg >> 16) & 0xFF;

    if (class_code == 0x01 && subclass == 0x08) {
        uint32_t bar0_low = pcie_read32(b, d, f, 0x10);
        uint32_t bar0_high = pcie_read32(b, d, f, 0x14);
        
        uint64_t mmio_physical = (bar0_low & 0xFFFFFFF0);
        if ((bar0_low & 0x6) == 0x4) {
            mmio_physical |= ((uint64_t)bar0_high << 32);
        }

        uintptr_t mmio_virtual = get_hhdm_virtual(mmio_physical);

        uint32_t command_reg = pcie_read32(b, d, f, 0x04);
        command_reg |= (1 << 1) | (1 << 2); 
        pcie_write32(b, d, f, 0x04, command_reg);

        nvme_init(mmio_virtual);

        uint64_t identify_phys_page = allocate_limine_physical_page();
        nvme_identify_drive(identify_phys_page);
    }
}
