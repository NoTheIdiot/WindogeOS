#ifndef PORTS_H
#define PORTS_H

// include files for some reason
#include <stdint.h>
#include <stddef.h>

// actual functions
void ports_outb(uint16_t port, uint8_t val);
uint8_t ports_inb(uint16_t port);
uint16_t ports_inw(uint16_t port);
void ports_outw(uint16_t port, uint16_t data);

// port hex values
#define ATA_DATA        0x1F0
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_SEL   0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7

#endif
