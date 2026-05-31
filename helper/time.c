// include files
#include <stdint.h>
#include <stddef.h>
#include "ports.h"
#include "dogeio.h"
#include "string.h"

// global variables for RTC
volatile unsigned long rtc_tick = 0;

struct {
    uint8_t second, minute, hour, day, month;
    uint32_t year;
} time;

uint8_t time_read_rtc(int reg) {
    ports_outb(0x70, reg);
    return ports_inb(0x71);
}

void time_rtc_handler() {
    rtc_tick++;
    dogeio_print(".");
    ports_outb(0x70, 0x0C);
    ports_inb(0x71);
}

/* This kept breaking because of stupid IDT
void time_rtc_init(uint8_t rate) {
    __asm__ volatile("cli");
    
    ports_outb(0x70, 0x8B);
    uint8_t prevB = ports_inb(0x71);
    ports_outb(0x70, 0x8B);
    ports_outb(0x71, prevB | 0x40);
    
    ports_outb(0x70, 0x8A);
    uint8_t prevA = ports_inb(0x71);
    ports_outb(0x70, 0x8A);
    ports_outb(0x71, (prevA & 0xF0) | (rate & 0x0F));
    
    ports_outb(0x70, 0x0C);
    ports_inb(0x71);

    __asm__ volatile("sti");
}*/

void time_update_time() {
    while (time_read_rtc(0x0A) & 0x80);

    time.second = time_read_rtc(0x00);
    time.minute = time_read_rtc(0x02);
    time.hour   = time_read_rtc(0x04);
    time.day    = time_read_rtc(0x07);
    time.month  = time_read_rtc(0x08);
    time.year   = time_read_rtc(0x09);

    uint8_t status_b = time_read_rtc(0x0B);

    if (!(status_b & 0x04)) {
        time.second = (time.second & 0x0F) + ((time.second / 16) * 10);
        time.minute = (time.minute & 0x0F) + ((time.minute / 16) * 10);
        time.hour   = ((time.hour & 0x0F) + (((time.hour & 0x70) / 16) * 10)) | (time.hour & 0x80);
        time.day    = (time.day & 0x0F) + ((time.day / 16) * 10);
        time.month  = (time.month & 0x0F) + ((time.month / 16) * 10);
        time.year   = (time.year & 0x0F) + ((time.year / 16) * 10);
    }

    if (!(status_b & 0x02) && (time.hour & 0x80)) {
        time.hour = ((time.hour & 0x7F) + 12) % 24;
    }
}

void time_show() {
    char buffer[16];
    time_update_time();

    if (time.hour < 10) dogeio_print("0");
    string_itoa(time.hour, buffer);
    dogeio_print(buffer);
    dogeio_print(":");

    if (time.minute < 10) dogeio_print("0");
    string_itoa(time.minute, buffer);
    dogeio_print(buffer);
    dogeio_print(":");

    if (time.second < 10) dogeio_print("0");
    string_itoa(time.second, buffer);
    dogeio_print(buffer);
}

char* time_get_raw() {
    time_update_time();

    static char raw_buffer[16];
    int i = 0;

    raw_buffer[i++] = (time.hour / 10) + '0';
    raw_buffer[i++] = (time.hour % 10) + '0';
    raw_buffer[i++] = ':';

    raw_buffer[i++] = (time.minute / 10) + '0';
    raw_buffer[i++] = (time.minute % 10) + '0';
    raw_buffer[i++] = ':';

    raw_buffer[i++] = (time.second / 10) + '0';
    raw_buffer[i++] = (time.second % 10) + '0';
    raw_buffer[i++] = '\0';

    return raw_buffer;
}

void time_wait_sec(unsigned long seconds) {
    unsigned long start = rtc_tick;
    unsigned long target = start + (seconds * 2);
    while (rtc_tick < target) { 
        __asm__ volatile("hlt"); 
    }
}
