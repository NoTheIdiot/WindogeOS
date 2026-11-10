#include <system.h>
#include <stdint.h>
#include <dogeio.h>
#include <string.h>

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
} acpi_fadt_min_t;

// find the rsdp stack or idk exactly
static acpi_rsdp_t *find_rsdp(void) {
    for (uintptr_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (memcmp((void *)addr, "RSD PTR ", 8) == 0) {
            return (acpi_rsdp_t *)addr;
        }
    }
    return NULL;
}

void reboot(void) {
    asm volatile ("cli");

    // kindly ask the system to reboot via guess what, MORE PORTS.
    // i wonder who invented all of these outb and inb
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

void shutdown(void) {
    // disable interrupts so nothing does wrong
    asm volatile ("cli");

    acpi_rsdp_t *rsdp = find_rsdp();
    if (rsdp != NULL) {
        acpi_sdt_t *rsdt = (acpi_sdt_t *)(uintptr_t)rsdp->rsdt_addr;
        int entries = (rsdt->length - sizeof(acpi_sdt_t)) / 4;
        uint32_t *ptrs = (uint32_t *)((uintptr_t)rsdt + sizeof(acpi_sdt_t));

        for (int i = 0; i < entries; i++) {
            acpi_sdt_t *hdr = (acpi_sdt_t *)(uintptr_t)ptrs[i];
            if (memcmp(hdr->signature, "FACP", 4) == 0) {
                acpi_fadt_min_t *fadt = (acpi_fadt_min_t *)hdr;
                acpi_sdt_t *dsdt = (acpi_sdt_t *)(uintptr_t)fadt->dsdt;

                if (fadt->smi_command_port != 0 && fadt->acpi_enable != 0) {
                    if ((inw(fadt->pm1a_control_block) & 1) == 0) {
                        outb(fadt->smi_command_port, fadt->acpi_enable);
                    }
                }

                uint8_t *s5 = (uint8_t *)dsdt + sizeof(acpi_sdt_t);
                uint8_t *end = (uint8_t *)dsdt + dsdt->length - 4;
                while (s5 < end) {
                    if (memcmp(s5, "_S5_", 4) == 0) {
                        s5 += 5;
                        s5 += ((*s5 & 0xC0) >> 6) + 2;
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
        }
    }

    halt();
}
