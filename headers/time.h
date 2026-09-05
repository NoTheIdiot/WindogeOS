#ifndef TIME_H
#define TIME_H

#include <stdint.h>

void time_rtc_init(void);
void time_update_time(void);
uint8_t time_read_rtc(uint8_t reg);
char* time_get(void);
char* date_get(void);

#endif
