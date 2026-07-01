#ifndef TIME_H
#define TIME_H

#include <stdint.h>

extern volatile unsigned long rtc_tick;

extern struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
} time;

uint8_t time_read_rtc(int reg);
void time_rtc_handler(void);
void time_update_time(void);
void time_show(void);
char* time_get(void);
void time_wait_sec(unsigned long seconds);
void time_rtc_init(void);

#endif
