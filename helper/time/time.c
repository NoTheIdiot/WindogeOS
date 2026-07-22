#include <time.h>
#include <string.h>
#include <basicutil.h>

struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
} time;

uint8_t time_read_rtc(uint8_t reg) {
    ports_outb(0x70, reg);
    return ports_inb(0x71);
}

void time_rtc_init(void) {
    __asm__ volatile("cli");
    
    ports_outb(0x70, 0x8A);
    uint8_t prev = ports_inb(0x71);
    ports_outb(0x70, 0x8A);
    ports_outb(0x71, (uint8_t)((prev & 0xF0) | 0x0F)); 

    __asm__ volatile("sti");
}

void time_update_time(void) {
    while (time_read_rtc(0x0A) & 0x80);

    time.second = time_read_rtc(0x00);
    time.minute = time_read_rtc(0x02);
    time.hour   = time_read_rtc(0x04);
    time.day    = time_read_rtc(0x07);
    time.month  = time_read_rtc(0x08);
    time.year   = time_read_rtc(0x09);

    uint8_t status_b = time_read_rtc(0x0B);

    if (!(status_b & 0x04)) {
        time.second = (uint8_t)((time.second & 0x0F) + ((time.second / 16) * 10));
        time.minute = (uint8_t)((time.minute & 0x0F) + ((time.minute / 16) * 10));
        time.hour   = (uint8_t)(((time.hour & 0x0F) + (((time.hour & 0x70) / 16) * 10)) | (time.hour & 0x80));
        time.day    = (uint8_t)((time.day & 0x0F) + ((time.day / 16) * 10));
        time.month  = (uint8_t)((time.month & 0x0F) + ((time.month / 16) * 10));
        time.year   = (uint32_t)((time.year & 0x0F) + ((time.year / 16) * 10));
    }

    if (!(status_b & 0x02) && (time.hour & 0x80)) {
        time.hour = (uint8_t)(((time.hour & 0x7F) + 12) % 24);
    }
}

char* time_get(void) {
    time_update_time();

    static char raw_buffer[16];
    int i = 0;

    raw_buffer[i++] = (char)((time.hour / 10) + '0');
    raw_buffer[i++] = (char)((time.hour % 10) + '0');
    raw_buffer[i++] = ':';

    raw_buffer[i++] = (char)((time.minute / 10) + '0');
    raw_buffer[i++] = (char)((time.minute % 10) + '0');
    raw_buffer[i++] = ':';

    raw_buffer[i++] = (char)((time.second / 10) + '0');
    raw_buffer[i++] = (char)((time.second % 10) + '0');
    raw_buffer[i++] = '\0';

    return raw_buffer;
}
