#include "system.h"

#define AHCI_FIS_TYPE_REG_H2D 0x27
#define AHCI_CMD_SLOT_READ    0
#define AHCI_CMD_SLOT_WRITE   0

typedef struct {
    uint8_t  fis_type;
    uint8_t  pmport:4;
    uint8_t  rsv0:3;
    uint8_t  c:1;
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  rsv1;
} __attribute__((packed)) fis_reg_h2d_t;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc:22;
    uint32_t rsv1:9;
    uint32_t i:1;
} __attribute__((packed)) ahci_prdt_t;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    ahci_prdt_t prdt;
} __attribute__((packed)) ahci_cmd_table_t;

typedef struct {
    uint8_t  cfl:5;
    uint8_t  a:1;
    uint8_t  w:1;
    uint8_t  p:1;
    uint8_t  v:1;
    uint8_t  pmp:4;
    uint8_t  rsv0:3;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1;
} __attribute__((packed)) ahci_cmd_header_t;

typedef struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1;
    uint32_t vendor[4];
} __attribute__((packed)) ahci_port_t;

typedef struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  rsv[116];
    uint8_t  vendor[96];
    ahci_port_t ports[32];
} __attribute__((packed)) ahci_hba_t;

static ahci_hba_t *hba = NULL;
static uintptr_t cmd_base_phys = 0;
static ahci_cmd_header_t *cmd_header_virt = NULL;
static ahci_cmd_table_t *cmd_table_virt = NULL;
static uint32_t active_port = 0xFFFFFFFF;

void ahci_sata_init(uintptr_t ecam_base_addr) {
    hba = (ahci_hba_t *)ecam_base_addr;
    
    cmd_base_phys = pmm_alloc_zeroed_page();
    
    uintptr_t tracking_page_virtual = 0x7FFFF0000000ULL;
    map_kernel_mmio_page(tracking_page_virtual, cmd_base_phys);
    
    cmd_header_virt = (ahci_cmd_header_t *)tracking_page_virtual;
    cmd_table_virt = (ahci_cmd_table_t *)(tracking_page_virtual + 1024);

    uint32_t pi = mmio_read32((uintptr_t)&hba->pi);
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            ahci_port_t *port = &hba->ports[i];
            uint32_t ssts = mmio_read32((uintptr_t)&port->ssts);
            if ((ssts & 0x0F) == 3 && ((ssts >> 8) & 0x0F) == 1) {
                if (mmio_read32((uintptr_t)&port->sig) == 0x00000101) {
                    uint32_t cmd = mmio_read32((uintptr_t)&port->cmd);
                    cmd &= ~(1 << 0);
                    cmd &= ~(1 << 4);
                    mmio_write32((uintptr_t)&port->cmd, cmd);
                    while (mmio_read32((uintptr_t)&port->cmd) & ((1 << 14) | (1 << 15)));

                    mmio_write32((uintptr_t)&port->clb, (uint32_t)cmd_base_phys);
                    mmio_write32((uintptr_t)&port->clbu, (uint32_t)(cmd_base_phys >> 32));
                    
                    uintptr_t fis_phys = cmd_base_phys + 2048;
                    mmio_write32((uintptr_t)&port->fb, (uint32_t)fis_phys);
                    mmio_write32((uintptr_t)&port->fbu, (uint32_t)(fis_phys >> 32));

                    mmio_write32((uintptr_t)&port->cmd, cmd | (1 << 4) | (1 << 0));
                    active_port = i;
                    break;
                }
            }
        }
    }
}


int ahci_exec_cmd(uint32_t port_num, uint64_t lba, uint32_t count, uint8_t write, uintptr_t buf_phys) {
    if (port_num == 0xFFFFFFFF) return 0;
    ahci_port_t *port = &hba->ports[port_num];
    
    mmio_write32((uintptr_t)&port->is, 0xFFFFFFFF);

    uint32_t slot = write ? AHCI_CMD_SLOT_WRITE : AHCI_CMD_SLOT_READ;
    cmd_header_virt[slot].cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_header_virt[slot].w = write ? 1 : 0;
    cmd_header_virt[slot].prdtl = 1;
    
    uintptr_t table_phys = cmd_base_phys + 1024;
    cmd_header_virt[slot].ctba = (uint32_t)table_phys;
    cmd_header_virt[slot].ctbau = (uint32_t)(table_phys >> 32);

    uint8_t *raw_table = (uint8_t *)cmd_table_virt;
    for (size_t i = 0; i < sizeof(ahci_cmd_table_t); i++) raw_table[i] = 0;

    cmd_table_virt->prdt.dba = (uint32_t)buf_phys;
    cmd_table_virt->prdt.dbau = (uint32_t)(buf_phys >> 32);
    cmd_table_virt->prdt.dbc = (count * 512) - 1;
    cmd_table_virt->prdt.i = 1;

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(&cmd_table_virt->cfis);
    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = write ? 0x35 : 0x25;
    
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6;
    
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    
    fis->countl = (uint8_t)(count & 0xFF);
    fis->counth = (uint8_t)((count >> 8) & 0xFF);

    while (mmio_read32((uintptr_t)&port->tfd) & (0x80 | 0x08));

    mmio_write32((uintptr_t)&port->ci, 1 << slot);

    while (1) {
        if ((mmio_read32((uintptr_t)&port->ci) & (1 << slot)) == 0) break;
        if (mmio_read32((uintptr_t)&port->is) & (1 << 30)) return 0;
    }

    return (mmio_read32((uintptr_t)&port->is) & (1 << 30)) ? 0 : 1;
}