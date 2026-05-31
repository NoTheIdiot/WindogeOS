#ifndef TIME_H
#define TIME_H

#include <stdint.h>

void time_read_rtc(int reg);
void time_rtc_handler();
void time_rtc_init(uint8_t);
void time_update_time();
void time_show();
char* time_get_raw();
void time_wait_ms(unsigned long ms);

#endif
