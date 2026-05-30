#ifndef TIME_H
#define TIME_H

void time_read_rtc(int reg);
void time_rtc_handler();
void time_update_time();
void time_show();
char* time_get_raw();
void time_wait_ms(unsigned long ms, unsigned long tick_ms);

#endif
