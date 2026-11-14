#include <system.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limine.h>

extern volatile struct limine_hhdm_request hhdm_request;

#define NVME_CAP    0x0000
#define NVME_VS     0x0008
#define NVME_CC     0x0014
#define NVME_CSTS   0x001C
#define NVME_AQA    0x0024
#define NVME_ASQ    0x0028
#define NVME_ACQ    0x0030

#define NVME_CC_EN      (1U << 0)
#define NVME_CSTS_RDY   (1U << 0)

#define NVME_ADMIN_Q_DEPTH 16

typedef struct __attribute__((packed))
{
    uint8_t opcode;
    uint8_t flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t rsvd0;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_cmd_t;

typedef struct __attribute__((packed))
{
    uint32_t result;
    uint32_t rsvd;

    uint16_t sq_head;
    uint16_t sq_id;

    uint16_t cid;
    uint16_t status;
} nvme_cpl_t;

typedef struct __attribute__((packed))
{
    uint8_t opcode;
    uint8_t flags;
    uint16_t cid;

    uint32_t nsid;

    uint64_t reserved0;
    uint64_t metadata;

    uint64_t prp1;
    uint64_t prp2;

    uint64_t slba;

    uint16_t nlb;
    uint16_t control;

    uint32_t dsmgmt;

    uint32_t reftag;
    uint16_t apptag;
    uint16_t appmask;
} nvme_rw_cmd_t;

typedef struct
{
    volatile uint8_t *regs;

    nvme_cmd_t *admin_sq;
    nvme_cpl_t *admin_cq;

    nvme_cmd_t *io_sq;
    nvme_cpl_t *io_cq;

    uint64_t admin_sq_phys;
    uint64_t admin_cq_phys;

    uint64_t io_sq_phys;
    uint64_t io_cq_phys;

    uint16_t sq_tail;
    uint16_t cq_head;
    uint16_t phase;

    uint16_t io_sq_tail;
    uint16_t io_cq_head;
    uint16_t io_phase;

    uint32_t nsid;

    bool present;
} nvme_controller_t;

static nvme_controller_t nvme;

// Forward declarations
static int nvme_create_io_cq(void);
static int nvme_create_io_sq(void);

static int nvme_submit_io(nvme_cmd_t *cmd)
{
    uint16_t slot = nvme.io_sq_tail;

    memcpy(&nvme.io_sq[slot], cmd, sizeof(nvme_cmd_t));

    nvme.io_sq_tail = (nvme.io_sq_tail + 1) % NVME_ADMIN_Q_DEPTH;

    mmio_write32((uintptr_t)(nvme.regs + 0x1008), nvme.io_sq_tail);

    while (1)
    {
        nvme_cpl_t *cpl = &nvme.io_cq[nvme.io_cq_head];

        if (((cpl->status >> 15) & 1) == nvme.io_phase)
            break;

        asm volatile("pause");
    }

    nvme.io_cq_head++;

    if (nvme.io_cq_head >= NVME_ADMIN_Q_DEPTH)
    {
        nvme.io_cq_head = 0;
        nvme.io_phase ^= 1;
    }

    mmio_write32((uintptr_t)(nvme.regs + 0x100C), nvme.io_cq_head);

    return 0;
}

static bool pci_find_nvme(uint8_t *out_bus,
                          uint8_t *out_dev,
                          uint8_t *out_func,
                          uint64_t *out_bar0)
{
    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t dev = 0; dev < 32; dev++)
        {
            for (uint8_t func = 0; func < 8; func++)
            {
                uint32_t classreg = pcie_read32(bus, dev, func, 0x08);

                if (classreg == 0xFFFFFFFF)
                    continue;

                uint8_t class_code = classreg >> 24;
                uint8_t subclass   = classreg >> 16;
                uint8_t progif     = classreg >> 8;

                if (class_code == 0x01 && subclass == 0x08 && progif == 0x02) {
                    uint32_t bar_low  = pcie_read32(bus, dev, func, 0x10);
                    uint32_t bar_high = pcie_read32(bus, dev, func, 0x14);

                    *out_bar0 = ((uint64_t)bar_high << 32) | (bar_low & ~0xFULL);

                    *out_bus = bus;
                    *out_dev = dev;
                    *out_func = func;

                    return true;
                }
            }
        }
    }

    return false;
}

static inline uint32_t nvme_read32(uint32_t off)
{
    return mmio_read32((uintptr_t)(nvme.regs + off));
}

static inline uint64_t nvme_read64(uint32_t off)
{
    return mmio_read64((uintptr_t)(nvme.regs + off));
}

static inline void nvme_write32(uint32_t off, uint32_t value)
{
    mmio_write32((uintptr_t)(nvme.regs + off), value);
}

static inline void nvme_write64(uint32_t off, uint64_t value)
{
    mmio_write64((uintptr_t)(nvme.regs + off), value);
}

static bool nvme_alloc_admin_queues(void)
{
    uint64_t hhdm = hhdm_request.response->offset;

    nvme.admin_sq_phys = pmm_alloc_zeroed_page();
    nvme.admin_cq_phys = pmm_alloc_zeroed_page();

    if (!nvme.admin_sq_phys || !nvme.admin_cq_phys)
        return false;

    nvme.admin_sq = (nvme_cmd_t *)(uintptr_t)(nvme.admin_sq_phys + hhdm);
    nvme.admin_cq = (nvme_cpl_t *)(uintptr_t)(nvme.admin_cq_phys + hhdm);

    return true;
}

static bool nvme_enable_controller(void)
{
    nvme_write32(NVME_CC, 0);

    while (nvme_read32(NVME_CSTS) & NVME_CSTS_RDY)
        asm volatile("pause");

    nvme_write32(NVME_AQA, ((NVME_ADMIN_Q_DEPTH - 1) << 16) | (NVME_ADMIN_Q_DEPTH - 1));

    nvme_write64(NVME_ASQ, nvme.admin_sq_phys);
    nvme_write64(NVME_ACQ, nvme.admin_cq_phys);

    // Fixed IOCQES to 4 (2^4 = 16 bytes, matching nvme_cpl_t size) and IOSQES to 6 (2^6 = 64 bytes)
    nvme_write32(NVME_CC, (4 << 20) | (6 << 16) | NVME_CC_EN);

    while (!(nvme_read32(NVME_CSTS) & NVME_CSTS_RDY))
        asm volatile("pause");

    return true;
}

static int nvme_submit_admin(nvme_cmd_t *cmd)
{
    uint16_t slot = nvme.sq_tail;

    memcpy(&nvme.admin_sq[slot], cmd, sizeof(nvme_cmd_t));

    nvme.sq_tail = (nvme.sq_tail + 1) % NVME_ADMIN_Q_DEPTH;

    mmio_write32((uintptr_t)(nvme.regs + 0x1000), nvme.sq_tail);

    while (1)
    {
        nvme_cpl_t *cpl = &nvme.admin_cq[nvme.cq_head];

        if (((cpl->status >> 15) & 1) == nvme.phase)
            break;

        asm volatile("pause");
    }

    nvme.cq_head++;

    if (nvme.cq_head >= NVME_ADMIN_Q_DEPTH)
    {
        nvme.cq_head = 0;
        nvme.phase ^= 1;
    }

    mmio_write32((uintptr_t)(nvme.regs + 0x1004), nvme.cq_head);

    return 0;
}

static int nvme_identify_controller(void) {
    uint64_t identify_phys = pmm_alloc_zeroed_page();

    if (!identify_phys)
        return -1;

    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = 0x06;
    cmd.prp1   = identify_phys;
    cmd.cdw10  = 1;

    return nvme_submit_admin(&cmd);
}

static int nvme_identify_namespace(void)
{
    uint64_t identify_phys = pmm_alloc_zeroed_page();

    if (!identify_phys)
        return -1;

    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = 0x06;
    cmd.nsid   = 1;
    cmd.prp1   = identify_phys;
    cmd.cdw10  = 0;

    return nvme_submit_admin(&cmd);
}

bool nvme_init(void)
{
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint64_t bar0;

    if (!pci_find_nvme(&bus, &dev, &func, &bar0))
        return false;

    uint32_t command = pcie_read32(bus, dev, func, 0x04);

    command |= (1 << 1);
    command |= (1 << 2);

    pcie_write32(bus, dev, func, 0x04, command);

    for (uint64_t off = 0; off < 0x4000; off += PAGE_SIZE)
        map_kernel_mmio_page(0xFFFFFFFFA1000000ULL + off, bar0 + off);

    nvme.regs = (volatile uint8_t *)0xFFFFFFFFA1000000ULL;

    if (!nvme_alloc_admin_queues())
        return false;

    if (!nvme_enable_controller())
        return false;

    nvme.sq_tail = 0;
    nvme.cq_head = 0;
    nvme.phase = 1;

    nvme.io_sq_tail = 0;
    nvme.io_cq_head = 0;
    nvme.io_phase = 1;

    nvme.nsid = 1;

    if (nvme_identify_controller())
        return false;

    if (nvme_identify_namespace())
        return false;

    if (nvme_create_io_cq())
        return false;

    if (nvme_create_io_sq())
        return false;

    nvme.present = true;

    return true;
}

int nvme_read_sector(uint64_t lba, uint8_t *buffer)
{
    uint64_t hhdm = hhdm_request.response->offset;

    nvme_rw_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = 0x02;
    cmd.cid = 1;
    cmd.nsid = 1;

    cmd.prp1 = (uint64_t)((uintptr_t)buffer - hhdm);

    cmd.slba = lba;
    cmd.nlb = 0;

    return nvme_submit_io((nvme_cmd_t *)&cmd);
}

static int nvme_create_io_cq(void)
{
    uint64_t hhdm = hhdm_request.response->offset;

    nvme.io_cq_phys = pmm_alloc_zeroed_page();

    if (!nvme.io_cq_phys)
        return -1;

    nvme.io_cq = (nvme_cpl_t *)(uintptr_t)(nvme.io_cq_phys + hhdm);

    nvme_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = 0x05;
    cmd.prp1 = nvme.io_cq_phys;

    cmd.cdw10 = ((NVME_ADMIN_Q_DEPTH - 1) << 16) | 1;
    // PC = 1 (Physically Contiguous), IEN = 0 (Interrupts disabled for polling mode)
    cmd.cdw11 = 1;

    return nvme_submit_admin(&cmd);
}

static int nvme_create_io_sq(void)
{
    uint64_t hhdm = hhdm_request.response->offset;

    nvme.io_sq_phys = pmm_alloc_zeroed_page();

    if (!nvme.io_sq_phys)
        return -1;

    nvme.io_sq = (nvme_cmd_t *)(uintptr_t)(nvme.io_sq_phys + hhdm);

    nvme_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = 0x01;
    cmd.prp1 = nvme.io_sq_phys;

    cmd.cdw10 = ((NVME_ADMIN_Q_DEPTH - 1) << 16) | 1;
    cmd.cdw11 = (1 << 16) | 1;

    return nvme_submit_admin(&cmd);
}

int nvme_write_sector(uint64_t lba, const uint8_t *buffer)
{
    uint64_t hhdm = hhdm_request.response->offset;

    uint64_t phys =
        (uint64_t)((uintptr_t)buffer - hhdm);

    nvme_rw_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = 0x01;
    cmd.cid    = 2;

    cmd.nsid   = 1;

    cmd.prp1   = phys;

    cmd.slba   = lba;

    cmd.nlb    = 0;

    return nvme_submit_io((nvme_cmd_t *)&cmd);
}