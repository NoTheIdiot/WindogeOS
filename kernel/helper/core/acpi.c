#include <system.h>
#include <stdint.h>
#include <dogeio.h>
#include <string.h>
#include <limine.h>

extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_hhdm_request hhdm_request;

typedef struct __attribute__((packed)) {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} acpi_rsdp_t;

typedef struct __attribute__((packed)) {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_t;

typedef struct __attribute__((packed)) {
    acpi_sdt_t header;
    uint32_t   firmware_ctrl;
    uint32_t   dsdt;
    uint8_t    reserved;
    uint8_t    preferred_pm_profile;
    uint16_t   sci_interrupt;
    uint32_t   smi_command_port;
    uint8_t    acpi_enable;
    uint8_t    acpi_disable;
    uint8_t    s4bios_req;
    uint8_t    pstate_control;
    uint32_t   pm1a_event_block;
    uint32_t   pm1b_event_block;
    uint32_t   pm1a_control_block;
    uint32_t   pm1b_control_block;
    uint32_t   pm2_control_block;
    uint32_t   pm_timer_block;
    uint32_t   gpe0_block;
    uint32_t   gpe1_block;
    uint8_t    pm1_event_length;
    uint8_t    pm1_control_length;
    uint8_t    pm2_control_length;
    uint8_t    pm_timer_length;
    uint8_t    gpe0_length;
    uint8_t    gpe1_length;
    uint8_t    gpe1_base;
    uint8_t    cstate_control;
    uint16_t   worst_c2_latency;
    uint16_t   worst_c3_latency;
    uint16_t   flush_size;
    uint16_t   flush_stride;
    uint8_t    duty_offset;
    uint8_t    duty_width;
    uint8_t    day_alarm;
    uint8_t    month_alarm;
    uint8_t    century;
    uint16_t   boot_architecture_flags;
    uint8_t    reserved2;
    uint32_t   flags;
    uint8_t    reset_reg[12];
    uint8_t    reset_value;
    uint8_t    reserved3[3];
    uint64_t   x_firmware_ctrl;
    uint64_t   x_dsdt;
} acpi_fadt_min_t;

static inline void *phys_to_virt(uint64_t phys) {
    if (phys == 0 || hhdm_request.response == NULL) return NULL;
    return (void *)(uintptr_t)(phys + hhdm_request.response->offset);
}

// find the rsdp stack or idk exactly
static acpi_rsdp_t *find_rsdp(void) {
    if (rsdp_request.response != NULL && rsdp_request.response->address != NULL) {
        uintptr_t addr = (uintptr_t)rsdp_request.response->address;
        if (hhdm_request.response != NULL && addr < hhdm_request.response->offset) {
            addr += hhdm_request.response->offset;
        }
        return (acpi_rsdp_t *)addr;
    }

    if (hhdm_request.response == NULL) return NULL;
    uintptr_t hhdm = hhdm_request.response->offset;

    for (uintptr_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (memcmp((void *)(addr + hhdm), "RSD PTR ", 8) == 0) {
            return (acpi_rsdp_t *)(addr + hhdm);
        }
    }
    return NULL;
}

void reboot(void) {
    asm volatile ("cli");

    // kindly ask the system to reboot via guess what, MORE PORTS.
    // i wonder who invented all of these outb and inb
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);

    uint8_t status;
    int timeout = 100000;
    // wait till it reboots or timeouts
    do {
        status = inb(0x64);
    } while ((status & 0x02) && --timeout > 0);
    outb(0x64, 0xFE);

    // if it timeouts, loop forever.
    halt();
}

static void parse_dsdt_and_shutdown(acpi_fadt_min_t *fadt) {
    uint64_t dsdt_phys = fadt->dsdt;
    if (fadt->header.revision >= 2 && fadt->header.length >= sizeof(acpi_fadt_min_t) && fadt->x_dsdt != 0) {
        dsdt_phys = fadt->x_dsdt;
    }

    acpi_sdt_t *dsdt = (acpi_sdt_t *)phys_to_virt(dsdt_phys);
    if (dsdt == NULL) return;

    if (fadt->smi_command_port != 0 && fadt->acpi_enable != 0) {
        if ((inw(fadt->pm1a_control_block) & 1) == 0) {
            outb(fadt->smi_command_port, fadt->acpi_enable);
            int timeout = 100000;
            while (((inw(fadt->pm1a_control_block) & 1) == 0) && --timeout > 0);
        }
    }

    uint8_t *s5 = (uint8_t *)dsdt + sizeof(acpi_sdt_t);
    uint8_t *end = (uint8_t *)dsdt + dsdt->length - 4;
    while (s5 < end) {
        if (memcmp(s5, "_S5_", 4) == 0) {
            s5 += 4;
            if (*s5 != 0x12) {
                s5++;
                continue;
            }
            s5++;
            uint8_t pkg_len_bytes = ((*s5 & 0xC0) >> 6) + 1;
            s5 += pkg_len_bytes + 1;

            if (*s5 == 0x0A) s5++;
            uint16_t slp_typa = (*s5) << 10;
            s5++;
            if (*s5 == 0x0A) s5++;
            uint16_t slp_typb = (*s5) << 10;

            outw(fadt->pm1a_control_block, slp_typa | (1 << 13));
            if (fadt->pm1b_control_block != 0) {
                outw(fadt->pm1b_control_block, slp_typb | (1 << 13));
            }
            break;
        }
        s5++;
    }
}

void shutdown(void) {
    // disable interrupts so nothing does wrong
    asm volatile ("cli");

    acpi_rsdp_t *rsdp = find_rsdp();
    if (rsdp != NULL) {
        if (rsdp->revision >= 2 && rsdp->xsdt_addr != 0) {
            acpi_sdt_t *xsdt = (acpi_sdt_t *)phys_to_virt(rsdp->xsdt_addr);
            if (xsdt != NULL) {
                int entries = (xsdt->length - sizeof(acpi_sdt_t)) / 8;
                uint64_t *ptrs = (uint64_t *)((uintptr_t)xsdt + sizeof(acpi_sdt_t));
                for (int i = 0; i < entries; i++) {
                    acpi_sdt_t *hdr = (acpi_sdt_t *)phys_to_virt(ptrs[i]);
                    if (hdr != NULL && memcmp(hdr->signature, "FACP", 4) == 0) {
                        parse_dsdt_and_shutdown((acpi_fadt_min_t *)hdr);
                        break;
                    }
                }
            }
        } else if (rsdp->rsdt_addr != 0) {
            acpi_sdt_t *rsdt = (acpi_sdt_t *)phys_to_virt(rsdp->rsdt_addr);
            if (rsdt != NULL) {
                int entries = (rsdt->length - sizeof(acpi_sdt_t)) / 4;
                uint32_t *ptrs = (uint32_t *)((uintptr_t)rsdt + sizeof(acpi_sdt_t));
                for (int i = 0; i < entries; i++) {
                    acpi_sdt_t *hdr = (acpi_sdt_t *)phys_to_virt(ptrs[i]);
                    if (hdr != NULL && memcmp(hdr->signature, "FACP", 4) == 0) {
                        parse_dsdt_and_shutdown((acpi_fadt_min_t *)hdr);
                        break;
                    }
                }
            }
        }
    }

    halt();
}
