#ifndef DISK_H
#define DISK_H

#include <basicutil.h>
#include <stdint.h>
#include <stddef.h>

static inline int ata_Ready(void) {
    for (volatile int i = 0; i < 4; i++) {
        ports_inb(ATA_STATUS);
    }
    uint8_t status;
    do {
        status = ports_inb(ATA_STATUS);
        if (status & 0x21) return -1;
    } while ((status & 0x80) || !(status & 0x08));
    return 0;
}

#endif