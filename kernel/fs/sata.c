#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <io.h>
#include <limine.h>
#include <dogeio.h>

#define AHCI_MAX_PORTS        32
#define AHCI_FIS_TYPE_REG_H2D 0x27
#define AHCI_GHC_AE           (1U << 31)
#define AHCI_GHC_HR           (1U << 0)

extern uintptr_t alloc_page(void);
extern void halt(void);

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
    uint8_t  rsv1[4];
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
    uint8_t     cfis[64];
    uint8_t     acmd[16];
    uint8_t     rsv[48];
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
    uint32_t rsv1[4];
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
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed)) ahci_port_t;

typedef struct {
    uint32_t    cap;
    uint32_t    ghc;
    uint32_t    is;
    uint32_t    pi;
    uint32_t    vs;
    uint32_t    ccc_ctl;
    uint32_t    ccc_pts;
    uint32_t    em_loc;
    uint32_t    em_ctl;
    uint32_t    cap2;
    uint32_t    bohc;
    uint8_t     rsv[116];
    uint8_t     vendor[96];
    ahci_port_t ports[32];
} __attribute__((packed)) ahci_hba_t;

static ahci_hba_t *g_hba = NULL;
static uint8_t g_current_drive = 0;
static uint8_t g_drive_present[AHCI_MAX_PORTS];
static uint64_t g_drive_sector_counts[AHCI_MAX_PORTS];

static uintptr_t g_cmd_table_phys[AHCI_MAX_PORTS][32];
static uintptr_t g_cmd_phys[AHCI_MAX_PORTS];
static ahci_cmd_header_t *g_cmd_header_virt[AHCI_MAX_PORTS];
static ahci_cmd_table_t *g_cmd_table_virt[AHCI_MAX_PORTS];

static uintptr_t g_dma_bounce_phys = 0;
static uint8_t *g_dma_bounce_virt = NULL;

static int ahci_rw_sector_phys(uint8_t port_num, uint64_t lba, uint16_t count, uint8_t write, uintptr_t buf_phys) {
    if (!g_hba || port_num >= AHCI_MAX_PORTS || !g_drive_present[port_num] || count == 0) {
        return -1;
    }

    ahci_port_t *port = &g_hba->ports[port_num];
    ahci_cmd_header_t *hdr = g_cmd_header_virt[port_num];

    int slot = 0;

    uintptr_t tbl_virt;
    if (slot < 16) {
        tbl_virt = (uintptr_t)g_cmd_table_virt[port_num] + (slot * 256);
    } else {
        tbl_virt = (uintptr_t)g_cmd_table_virt[port_num] + PAGE_SIZE + ((slot - 16) * 256);
    }
    ahci_cmd_table_t *tbl = (ahci_cmd_table_t *)tbl_virt;

    mmio_write32((uintptr_t)&port->is, 0xFFFFFFFFU);
    mmio_write32((uintptr_t)&port->serr, 0xFFFFFFFFU);

    hdr[slot].cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    hdr[slot].w = write ? 1 : 0;
    hdr[slot].prdtl = 1;
    hdr[slot].prdbc = 0;

    uintptr_t tbl_phys = g_cmd_table_phys[port_num][slot];
    hdr[slot].ctba = (uint32_t)tbl_phys;
    hdr[slot].ctbau = (uint32_t)(tbl_phys >> 32);

    memset(tbl, 0, sizeof(ahci_cmd_table_t));

    uint32_t byte_count = (uint32_t)count * BLOCK_SIZE;
    tbl->prdt.dba = (uint32_t)buf_phys;
    tbl->prdt.dbau = (uint32_t)(buf_phys >> 32);
    tbl->prdt.dbc = byte_count - 1;
    tbl->prdt.i = 1;

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(&tbl->cfis[0]);
    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = write ? 0x35 : 0x25; // READ/WRITE SECTORS EXT

    fis->lba0 = (uint8_t)(lba & 0xFF);
    fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
    fis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    fis->device = 1 << 6;
    fis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    fis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    fis->lba5 = (uint8_t)((lba >> 40) & 0xFF);

    fis->countl = (uint8_t)(count & 0xFF);
    fis->counth = (uint8_t)((count >> 8) & 0xFF);

    uint32_t spin = 0;
    while ((mmio_read32((uintptr_t)&port->tfd) & (0x80U | 0x08U)) && spin < 1000000U) {
        spin++;
    }
    if (spin == 1000000U) {
        sprintf("[AHCI Debug] %s timeout on port %u: drive busy before command issue.\n",
               write ? "write" : "read", port_num);
        return -1;
    }

    mmio_write32((uintptr_t)&port->ci, 1U << slot);

    uint32_t timeout = 10000000U;
    while (timeout--) {
        if ((mmio_read32((uintptr_t)&port->ci) & (1U << slot)) == 0) {
            break;
        }
        if (mmio_read32((uintptr_t)&port->is) & (1U << 30)) {
            mmio_write32((uintptr_t)&port->is, 1U << 30);
            sprintf("[AHCI Debug] %s failed on port %u: task file error status (tfes).\n",
                   write ? "write" : "read", port_num);
            return -1;
        }
    }

    if (timeout == 0) {
        sprintf("[AHCI Debug] %s failed on port %u: controller timed out waiting for completion.\n",
               write ? "write" : "read", port_num);
        return -1;
    }

    return 0;
}

int ata_select_drive(uint8_t drive) {
    if (drive >= AHCI_MAX_PORTS || !g_drive_present[drive]) {
        return -1;
    }
    g_current_drive = drive;
    return 0;
}

uint8_t ata_current_drive(void) {
    return g_current_drive;
}

int ata_drive_present(uint8_t drive) {
    return drive < AHCI_MAX_PORTS && g_drive_present[drive] != 0;
}

uint64_t ata_drive_sector_count(uint8_t drive) {
    return ata_drive_present(drive) ? g_drive_sector_counts[drive] : 0;
}

int ata_read_sector(uint8_t drive, uint64_t lba, uint8_t *buffer) {
    if (buffer == NULL || !ata_drive_present(drive) || g_dma_bounce_virt == NULL) {
        sprintf("[AHCI Debug] Read rejected on drive %u: invalid buffer, uninitialized DMA, or missing drive.\n", drive);
        return -1;
    }
    g_current_drive = drive;

    if (ahci_rw_sector_phys(drive, lba, 1, 0, g_dma_bounce_phys) != 0) {
        sprintf("[AHCI Debug] Read failed on drive %u (LBA %llu): DMA transfer timed out or errored.\n",
               drive, (unsigned long long)lba);
        return -1;
    }

    memcpy(buffer, g_dma_bounce_virt, BLOCK_SIZE);
    return 0;
}

int ata_write_sector(uint8_t drive, uint64_t lba, const uint8_t *buffer) {
    if (buffer == NULL || !ata_drive_present(drive) || g_dma_bounce_virt == NULL) {
        sprintf("[AHCI Debug] Write rejected on drive %u: invalid buffer, uninitialized DMA, or missing drive.\n", drive);
        return -1;
    }
    g_current_drive = drive;

    memcpy(g_dma_bounce_virt, buffer, BLOCK_SIZE);
    return ahci_rw_sector_phys(drive, lba, 1, 1, g_dma_bounce_phys);
}

void ata_probe_devices(void) {
    for (uint8_t i = 0; i < AHCI_MAX_PORTS; i++) {
        g_drive_present[i] = 0;
        g_drive_sector_counts[i] = 0;
    }

    uint8_t bus = 0, slot = 0, func = 0;
    uint64_t bar5_phys = 0;
    if (!pci_find_ahci(&bus, &slot, &func, &bar5_phys) || bar5_phys == 0) {
        sprintf("[AHCI Debug] No AHCI controller found.\n");
        return;
    }

    pci_enable_device(bus, slot, func);
    bar5_phys &= ~0xFULL;

    sprintf("[AHCI Debug] Found controller at PCI %02x:%02x.%x, BAR5 phys=0x%llx\n",
           bus, slot, func, (unsigned long long)bar5_phys);

    void *hba_mapped = map_kernel_mmio(bar5_phys, PAGE_SIZE * 2);
    if (!hba_mapped) {
        sprintf("[AHCI Debug] Failed to map AHCI BAR5 MMIO (phys=0x%llx).\n", (unsigned long long)bar5_phys);
        return;
    }
    g_hba = (ahci_hba_t *)hba_mapped;

    sprintf("[AHCI Debug] CAP=0x%08x PI=0x%08x VS=0x%08x\n",
           mmio_read32((uintptr_t)&g_hba->cap),
           mmio_read32((uintptr_t)&g_hba->pi),
           mmio_read32((uintptr_t)&g_hba->vs));

    uint32_t ghc = mmio_read32((uintptr_t)&g_hba->ghc);
    ghc |= AHCI_GHC_AE;
    mmio_write32((uintptr_t)&g_hba->ghc, ghc);

    g_dma_bounce_phys = alloc_page();
    g_dma_bounce_virt = (uint8_t *)map_kernel_mmio(g_dma_bounce_phys, PAGE_SIZE);
    if (!g_dma_bounce_virt) {
        sprintf("[AHCI Debug] Failed to map DMA bounce buffer (phys=0x%llx).\n", (unsigned long long)g_dma_bounce_phys);
        return;
    }

    uint32_t pi = mmio_read32((uintptr_t)&g_hba->pi);
    int first_drive = -1;

    for (uint8_t i = 0; i < AHCI_MAX_PORTS; i++) {
        if (!(pi & (1U << i))) continue;

        ahci_port_t *port = &g_hba->ports[i];

        uint32_t ssts = mmio_read32((uintptr_t)&port->ssts);
        if ((ssts & 0x0F) == 0) {
            continue;
        }

        uint32_t cmd = mmio_read32((uintptr_t)&port->cmd);
        cmd &= ~(1U << 0);
        cmd &= ~(1U << 4);
        mmio_write32((uintptr_t)&port->cmd, cmd);

        int timeout = 1000;
        while ((mmio_read32((uintptr_t)&port->cmd) & ((1U << 14) | (1U << 15))) && timeout--);

        uint32_t sctl = mmio_read32((uintptr_t)&port->sctl);
        sctl = (sctl & ~0x0F) | 0x01;
        mmio_write32((uintptr_t)&port->sctl, sctl);

        for (volatile int d = 0; d < 100; d++);

        sctl = mmio_read32((uintptr_t)&port->sctl);
        sctl &= ~0x0F;
        mmio_write32((uintptr_t)&port->sctl, sctl);

        timeout = 1000;
        while (timeout--) {
            ssts = mmio_read32((uintptr_t)&port->ssts);
            if ((ssts & 0x0F) == 3) break;
        }

        if ((ssts & 0x0F) != 3) {
            sprintf("[AHCI Debug] Port %u link not active (SSTS=0x%08x).\n", i, ssts);
            continue;
        }

        mmio_write32((uintptr_t)&port->serr, 0xFFFFFFFFU);

        uintptr_t page_cl_fis = alloc_page();
        uintptr_t page_tables_1 = alloc_page();
        uintptr_t page_tables_2 = alloc_page();

        void *cl_fis_virt = map_kernel_mmio(page_cl_fis, PAGE_SIZE);
        void *tables_1_virt = map_kernel_mmio(page_tables_1, PAGE_SIZE);
        void *tables_2_virt = map_kernel_mmio(page_tables_2, PAGE_SIZE);
        if (!cl_fis_virt || !tables_1_virt || !tables_2_virt) {
            sprintf("[AHCI Debug] Port %u failed to map command/FIS pages.\n", i);
            continue;
        }

        memset(cl_fis_virt, 0, PAGE_SIZE);
        memset(tables_1_virt, 0, PAGE_SIZE);
        memset(tables_2_virt, 0, PAGE_SIZE);

        g_cmd_phys[i] = page_cl_fis;
        g_cmd_header_virt[i] = (ahci_cmd_header_t *)cl_fis_virt;
        g_cmd_table_virt[i] = (ahci_cmd_table_t *)tables_1_virt;

        mmio_write32((uintptr_t)&port->clb, (uint32_t)page_cl_fis);
        mmio_write32((uintptr_t)&port->clbu, (uint32_t)(page_cl_fis >> 32));

        uintptr_t fis_phys = page_cl_fis + 1024;
        mmio_write32((uintptr_t)&port->fb, (uint32_t)fis_phys);
        mmio_write32((uintptr_t)&port->fbu, (uint32_t)(fis_phys >> 32));

        for (int j = 0; j < 32; j++) {
            uintptr_t tbl_phys;
            if (j < 16) {
                tbl_phys = page_tables_1 + (j * 256);
            } else {
                tbl_phys = page_tables_2 + ((j - 16) * 256);
            }

            g_cmd_table_phys[i][j] = tbl_phys;
            g_cmd_header_virt[i][j].prdtl = 1;
            g_cmd_header_virt[i][j].ctba = (uint32_t)tbl_phys;
            g_cmd_header_virt[i][j].ctbau = (uint32_t)(tbl_phys >> 32);
        }

        uint32_t port_cmd = mmio_read32((uintptr_t)&port->cmd);
        port_cmd |= (1U << 4) | (1U << 0);
        mmio_write32((uintptr_t)&port->cmd, port_cmd);

        timeout = 1000;
        while (timeout--) {
            uint32_t tfd = mmio_read32((uintptr_t)&port->tfd);
            if (!(tfd & (0x80U | 0x08U))) break;
        }

        g_drive_present[i] = 1;
        g_drive_sector_counts[i] = 20971520ULL;
        sprintf("[AHCI Debug] Port %u ready (sectors=%llu).\n",
               i, (unsigned long long)g_drive_sector_counts[i]);
        if (first_drive == -1) {
            first_drive = i;
        }
    }

    if (first_drive != -1) {
        g_current_drive = (uint8_t)first_drive;
    }
}

void sata_init(uintptr_t ecam_base_addr) {
    (void)ecam_base_addr;
    ata_probe_devices();
}