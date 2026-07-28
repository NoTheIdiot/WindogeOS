/*
random notes:
 - to anyone who wants to modify and call it your own, pls do delete this to avoid
   your drama

This is just the development of an actualy file system, do not delete fs.c.
This is a part of dogeio header file.
*/

#include <dogeio.h>
#include <stdint.h>
#include <stddef.h>
#include <basicutil.h>

#define ATA_DATA        0x1F0
#define ATA_FEATURES    0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7


// function to wait till ata is ready.
// just waits till the port responses with... *something*
static void ata_ready(void) {
    while ((ports_inb(ATA_STATUS) & 0x80) || !(ports_inb(ATA_STATUS) & 0x40));
}