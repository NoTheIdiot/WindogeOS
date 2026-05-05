#ifndef TIME_H
#define TIME_H

void time_read_rtc(int reg);
void time_update_time();
void time_show();
char* time_get_raw();

#endif