#include <drivers.h>
#include <system.h>

static inline uint8_t ata_wait_busy() {
    uint8_t status;
    while ((status = lowlevel_ports_inb(0x1F7)) & 0x80);
    return status;
}

static inline int ata_wait_drq() {
    uint8_t status = ata_wait_busy();
    if ((status & 0x01) || !(status & 0x08)) {
        return 0;
    }
    return 1;
}

void fs_disk_write(uint32_t sector, const void* buffer) {
    const uint16_t* ptr = (const uint16_t*)buffer;

    ata_wait_busy();

    lowlevel_ports_outb(0x1F6, 0xE0 | ((sector >> 24) & 0x0F));
    lowlevel_ports_outb(0x1F2, 1);
    lowlevel_ports_outb(0x1F3, (uint8_t)sector);
    lowlevel_ports_outb(0x1F4, (uint8_t)(sector >> 8));
    lowlevel_ports_outb(0x1F5, (uint8_t)(sector >> 16));
    lowlevel_ports_outb(0x1F7, 0x30);

    if (!ata_wait_drq()) return;

    for (int i = 0; i < 256; i++) {
        lowlevel_ports_outw(0x1F0, ptr[i]);
    }

    ata_wait_busy();
}

void fs_disk_read(uint32_t sector, void* buffer) {
    uint16_t* ptr = (uint16_t*)buffer;

    ata_wait_busy();

    lowlevel_ports_outb(0x1F6, 0xE0 | ((sector >> 24) & 0x0F));
    lowlevel_ports_outb(0x1F2, 1);
    lowlevel_ports_outb(0x1F3, (uint8_t)sector);
    lowlevel_ports_outb(0x1F4, (uint8_t)(sector >> 8));
    lowlevel_ports_outb(0x1F5, (uint8_t)(sector >> 16));
    lowlevel_ports_outb(0x1F7, 0x20);

    if (!ata_wait_drq()) {
        for (int i = 0; i < 256; i++) ptr[i] = 0;
        return;
    }
    
    for (int i = 0; i < 256; i++) {
        ptr[i] = lowlevel_ports_inw(0x1F0);
    }
}
