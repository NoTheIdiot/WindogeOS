#include <drivers.h>
#include <system.h>

void fs_disk_write(uint32_t sector, const void* buffer) {
    uint16_t* ptr = (uint16_t*)buffer;

    lowlevel_ports_outb(0x1F2, 1);
    lowlevel_ports_outb(0x1F3, (uint8_t)sector);
    lowlevel_ports_outb(0x1F4, (uint8_t)(sector >> 8));
    lowlevel_ports_outb(0x1F5, (uint8_t)(sector >> 16));
    lowlevel_ports_outb(0x1F6, 0xE0 | ((sector >> 24) & 0x0F));
    lowlevel_ports_outb(0x1f7, 0x30);
}

void fs_disk_read(uint32_t sector, void* buffer) {
    uint16_t* ptr = (uint16_t*)buffer;
    lowlevel_ports_outb(0x1F2, 1);
    lowlevel_ports_outb(0x1F3, (uint8_t)sector);
    lowlevel_ports_outb(0x1F4, (uint8_t)(sector >> 8));
    lowlevel_ports_outb(0x1F5, (uint8_t)(sector >> 16));
    lowlevel_ports_outb(0x1F6, 0xE0 | ((sector >> 24) & 0x0F));
    while ((lowlevel_ports_inb(0x1F7) & 0x88) != 0x08);
    
    for (int i = 0; i < 256; i++) {
        ptr[i] = lowlevel_ports_inw(0x1F0);
    }
}