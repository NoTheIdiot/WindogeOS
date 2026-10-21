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

#define CALENDAR_FILE "/.calendar"

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
    static const int days_per_month[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month < 1 || month > 12) {
        return 30; // When will this ever be true lol
    }
    // Is it a leap year February?
    if ((month == 2 && (year % 4 == 0 && year % 100 != 0)) || (year % 400 == 0)) {
        return 29;
    }

    return days_per_month[month];
}

static void print_month_title(int month, int year) {

    static const char* const month_names[] = {
        "", "January ", "February ", "March ", "April ", "May ", "June ",
            "July ", "August ", "September ", "October ", "November ", "December "
    };
    print(month_names[month]);
    
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

static void format_date_key(int year, int month, int day, char *out_buf) {
    int y = year;
    out_buf[0] = (char)('0' + (y / 1000));
    y %= 1000;
    out_buf[1] = (char)('0' + (y / 100));
    y %= 100;
    out_buf[2] = (char)('0' + (y / 10));
    out_buf[3] = (char)('0' + (y % 10));
    out_buf[4] = '-';
    out_buf[5] = (char)('0' + (month / 10));
    out_buf[6] = (char)('0' + (month % 10));
    out_buf[7] = '-';
    out_buf[8] = (char)('0' + (day / 10));
    out_buf[9] = (char)('0' + (day % 10));
    out_buf[10]= '\0';
}

static void get_reminder_for_date(int year, int month, int day, char *out_msg, size_t max_len) {
    out_msg[0] = '\0';
    if (!file_exists(CALENDAR_FILE)) return;

    dogec_stat_t st = {0};
    if (stat(CALENDAR_FILE, &st) != 0 || st.size == 0 || st.is_dir) return;

    char file_buf[256];
    for (int i = 0; i < 256; i++) file_buf[i] = 0;

    uint64_t bytes_to_read = st.size < 255 ? st.size : 255;
    int64_t bytes_read = (int64_t)read_file(CALENDAR_FILE, file_buf, bytes_to_read);
    if (bytes_read <= 0 || bytes_read >= 256) return;
    file_buf[bytes_read] = '\0';

    char target_key[16];
    format_date_key(year, month, day, target_key);

    char *line = file_buf;
    while (*line) {
        char *nl = line;
        while (*nl && *nl != '\n' && *nl != '\r') nl++;
        char saved_char = *nl;
        *nl = '\0';

        if (str_strlen(line) > 11 && str_strncmp(line, target_key, 10) == 0 && line[10] == ':') {
            char *msg_ptr = line + 11;
            while (*msg_ptr == ' ') msg_ptr++;
            str_strncpy(out_msg, msg_ptr, max_len - 1);
            out_msg[max_len - 1] = '\0';
            break;
        }

        *nl = saved_char;
        line = nl;
        while (*line == '\n' || *line == '\r') line++;
    }
}

static void save_reminder_for_date(int year, int month, int day, const char *new_msg) {
    char target_key[16];
    format_date_key(year, month, day, target_key);

    char file_buf[256];
    for (int i = 0; i < 256; i++) file_buf[i] = 0;

    uint64_t fsize = 0;
    if (file_exists(CALENDAR_FILE)) {
        dogec_stat_t st = {0};
        if (stat(CALENDAR_FILE, &st) == 0 && st.size > 0 && !st.is_dir) {
            uint64_t bytes_to_read = st.size < 255 ? st.size : 255;
            int64_t br = (int64_t)read_file(CALENDAR_FILE, file_buf, bytes_to_read);
            if (br > 0 && br < 256) {
                file_buf[br] = '\0';
                fsize = (uint64_t)br;
            }
        }
    }

    char new_buf[256];
    for (int i = 0; i < 256; i++) new_buf[i] = 0;
    size_t new_buf_len = 0;

    bool updated = false;
    if (fsize > 0) {
        char *line = file_buf;
        while (*line && new_buf_len < 200) {
            char *nl = line;
            while (*nl && *nl != '\n' && *nl != '\r') nl++;
            char saved_char = *nl;
            *nl = '\0';

            if (str_strlen(line) > 11 && str_strncmp(line, target_key, 10) == 0 && line[10] == ':') {
                if (new_msg && new_msg[0] != '\0') {
                    size_t needed = 11 + str_strlen(new_msg) + 2;
                    if (new_buf_len + needed < 240) {
                        str_strcat(new_buf, target_key);
                        str_strcat(new_buf, ": ");
                        str_strcat(new_buf, new_msg);
                        str_strcat(new_buf, "\n");
                        new_buf_len = str_strlen(new_buf);
                    }
                    updated = true;
                }
            } else if (line[0] != '\0') {
                size_t l_len = str_strlen(line);
                if (new_buf_len + l_len + 2 < 240) {
                    str_strcat(new_buf, line);
                    str_strcat(new_buf, "\n");
                    new_buf_len = str_strlen(new_buf);
                }
            }

            *nl = saved_char;
            line = nl;
            while (*line == '\n' || *line == '\r') line++;
        }
    }

    if (!updated && new_msg && new_msg[0] != '\0') {
        size_t needed = 11 + str_strlen(new_msg) + 2;
        if (new_buf_len + needed < 240) {
            str_strcat(new_buf, target_key);
            str_strcat(new_buf, ": ");
            str_strcat(new_buf, new_msg);
            str_strcat(new_buf, "\n");
        }
    }

    if (!file_exists(CALENDAR_FILE)) {
        create_file(CALENDAR_FILE);
    }
    write_file(CALENDAR_FILE, new_buf);
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
    
    char reminder_buf[128];
    get_reminder_for_date(year, month, selected_day, reminder_buf, sizeof(reminder_buf));
    if (reminder_buf[0] != '\0') {
        print("Reminder: ");
        text_color(COLOR_BRIGHT_YELLOW);
        println(reminder_buf);
        text_color(COLOR_WHITE);
    } else {
        println("Reminder: (None)");
    }

    println("====================");
    println("Left/Right: Day     ");
    println("Up/Down:    Month   ");
    println("+/-:        Year    ");
    println("ENTER:      Edit Rem");
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
        } else if (key == KEY_ENTER || key == '\n' || key == 13) {
            clear();
            println("=== Edit Reminder ===");
            print("Date: ");
            char dkey[16];
            format_date_key(year, month, day, dkey);
            println(dkey);
            println("Type reminder text and press Enter.");
            println("(Leave blank and press Enter to clear)");
            print("> ");

            char input_rem[64];
            for (int i = 0; i < 64; i++) input_rem[i] = 0;
            int input_idx = 0;

            while (true) {
                uint64_t rkey = get_key();
                if (rkey == KEY_ENTER || rkey == '\n' || rkey == 13) {
                    break;
                } else if (rkey == KEY_BACKSPACE || rkey == 8 || rkey == 127) {
                    if (input_idx > 0) {
                        input_idx--;
                        input_rem[input_idx] = '\0';
                        print("\b \b");
                    }
                } else if (rkey >= 32 && rkey < 127 && input_idx < 60) {
                    input_rem[input_idx++] = (char)rkey;
                    input_rem[input_idx] = '\0';
                    char tmp[2] = {(char)rkey, '\0'};
                    print(tmp);
                }
            }

            save_reminder_for_date(year, month, day, input_rem);
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
