#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/stddef.h"
#include "../library/bool.h"
#include "../library/dogelib.h"

struct datetime {
    int year;
    int month;
    int day;
};

static int get_start_day_of_month(int year, int month) {
    if (month < 3) {
        month += 12;
        year -= 1;
    }
    int k = year % 100;
    int j = year / 100;
    int h = (1 + (13 * (month + 1)) / 5 + k + (k / 4) + (j / 4) + (5 * j)) % 7;
    return (h + 6) % 7;
}

static int get_days_in_month(int year, int month) {
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12:
            return 31;
        case 4: case 6: case 9: case 11:
            return 30;
        case 2:
            if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
                return 29;
            }
            return 28;
        default:
            return 30;
    }
}

static void print_month_title(int month, int year) {
    switch (month) {
        case 1: print("January "); break;
        case 2: print("February "); break;
        case 3: print("March "); break;
        case 4: print("April "); break;
        case 5: print("May "); break;
        case 6: print("June "); break;
        case 7: print("July "); break;
        case 8: print("August "); break;
        case 9: print("September "); break;
        case 10: print("October "); break;
        case 11: print("November "); break;
        case 12: print("December "); break;
        default: break;
    }
    char y_buf[16];
    int y = year;
    int idx = 0;
    char temp[16];
    int t_idx = 0;
    while (y > 0) {
        temp[t_idx++] = (char)('0' + (y % 10));
        y /= 10;
    }
    for (int i = t_idx - 1; i >= 0; i--) {
        y_buf[idx++] = temp[i];
    }
    y_buf[idx] = '\0';
    println(y_buf);
}

static void get_current_date(struct datetime *dt) {
    uint64_t raw_time = 0;
    get_time(&raw_time);
    if (raw_time == 0) {
        dt->year = 2026;
        dt->month = 10;
        dt->day = 18;
        return;
    }

    uint64_t days = raw_time / 86400;
    int y = 1970;
    while (true) {
        int leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        uint64_t d_in_year = leap ? 366 : 365;
        if (days >= d_in_year) {
            days -= d_in_year;
            y++;
        } else {
            break;
        }
    }
    dt->year = y;

    int m = 1;
    while (m <= 12) {
        int d_in_m = get_days_in_month(y, m);
        if (days >= (uint64_t)d_in_m) {
            days -= (uint64_t)d_in_m;
            m++;
        } else {
            break;
        }
    }
    dt->month = m;
    dt->day = (int)(days + 1);
}

static void render_calendar(int year, int month, int selected_day) {
    clear();
    println("====================");
    print("   ");
    print_month_title(month, year);
    println("====================");
    println("Su Mo Tu We Th Fr Sa");

    int start_day = get_start_day_of_month(year, month);
    int total_days = get_days_in_month(year, month);

    char line_buf[64];
    int line_idx = 0;

    for (int i = 0; i < start_day; i++) {
        line_buf[line_idx++] = ' ';
        line_buf[line_idx++] = ' ';
        line_buf[line_idx++] = ' ';
    }

    for (int day = 1; day <= total_days; day++) {
        if (day == selected_day) {
            if (line_idx > 0) {
                line_buf[line_idx] = '\0';
                print(line_buf);
                line_idx = 0;
            }
            text_color(COLOR_BRIGHT_GREEN);
            if (day < 10) {
                char tmp[4] = {' ', (char)('0' + day), ' ', '\0'};
                print(tmp);
            } else {
                char tmp[4] = {(char)('0' + (day / 10)), (char)('0' + (day % 10)), ' ', '\0'};
                print(tmp);
            }
            text_color(COLOR_WHITE);
        } else {
            if (day < 10) {
                line_buf[line_idx++] = ' ';
                line_buf[line_idx++] = (char)('0' + day);
            } else {
                line_buf[line_idx++] = (char)('0' + (day / 10));
                line_buf[line_idx++] = (char)('0' + (day % 10));
            }
            line_buf[line_idx++] = ' ';
        }

        if ((start_day + day) % 7 == 0 || day == total_days) {
            line_buf[line_idx] = '\0';
            println(line_buf);
            line_idx = 0;
        }
    }

    println("====================");
    println("Left/Right: Day     ");
    println("Up/Down:    Month   ");
    println("+/-:        Year    ");
    println("R:          Reset   ");
    println("ESC/Q:      Quit    ");
}

void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    struct datetime current_dt;
    get_current_date(&current_dt);

    int year = current_dt.year;
    int month = current_dt.month;
    int day = current_dt.day;

    render_calendar(year, month, day);

    while (true) {
        uint64_t key = get_key();
        int max_days = get_days_in_month(year, month);

        if (key == KEY_LEFT || key == 'a') {
            day--;
            if (day < 1) {
                month--;
                if (month < 1) {
                    month = 12;
                    year--;
                }
                day = get_days_in_month(year, month);
            }
        } else if (key == KEY_RIGHT || key == 'd') {
            day++;
            if (day > max_days) {
                day = 1;
                month++;
                if (month > 12) {
                    month = 1;
                    year++;
                }
            }
        } else if (key == KEY_UP || key == 'w') {
            month--;
            if (month < 1) {
                month = 12;
                year--;
            }
            int new_max = get_days_in_month(year, month);
            if (day > new_max) day = new_max;
        } else if (key == KEY_DOWN || key == 's') {
            month++;
            if (month > 12) {
                month = 1;
                year++;
            }
            int new_max = get_days_in_month(year, month);
            if (day > new_max) day = new_max;
        } else if (key == '+' || key == '=') {
            year++;
            int new_max = get_days_in_month(year, month);
            if (day > new_max) day = new_max;
        } else if (key == '-' || key == '_') {
            year--;
            int new_max = get_days_in_month(year, month);
            if (day > new_max) day = new_max;
        } else if (key == 'r' || key == 'R') {
            get_current_date(&current_dt);
            year = current_dt.year;
            month = current_dt.month;
            day = current_dt.day;
        } else if (key == 27 || key == 'q' || key == 'Q') {
            break;
        }

        render_calendar(year, month, day);
    }

    clear();
    sys_exit(0);
}