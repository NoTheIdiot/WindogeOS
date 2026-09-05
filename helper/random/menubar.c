#include <stdint.h>
#include <dogeio.h>
#include <boot/kernel.h>
#include <time.h>
#include <string.h>
#include <system.h>

#define TERMINAL_COLS 160

void menubar_draw() {
    uint32_t old_text_color = dogeio_text_color;
    uint32_t old_bg_color = dogeio_background_color;
    uint32_t old_cursor_x = cursor_x;
    uint32_t old_cursor_y = cursor_y;

    dogeio_text_color_change(0xFFFFFF);
    dogeio_text_background_change(0x282828);
    
    cursor_x = 0;
    cursor_y = 0;

    char menu_buffer[TERMINAL_COLS + 1];

    char* current_date = date_get();
    char* current_time = time_get();

    char right_section[32];
    int r_idx = 0;

    while (current_date[r_idx] != '\0') {
        right_section[r_idx] = current_date[r_idx];
        r_idx++;
    }

    right_section[r_idx++] = ' ';

    int t_idx = 0;
    while (current_time[t_idx] != '\0') {
        right_section[r_idx++] = current_time[t_idx++];
    }
    right_section[r_idx] = '\0';

    int right_len = r_idx;
    int left_section_target_len = TERMINAL_COLS - right_len - 1;

    dogeio_text_print(" ");
    str_pad(menu_buffer, windoge_version, left_section_target_len - 1, ' ');

    dogeio_text_print(menu_buffer);
    dogeio_text_print(right_section);
    dogeio_text_print(" ");

    dogeio_text_color_change(old_text_color);
    dogeio_text_background_change(old_bg_color);
    cursor_x = old_cursor_x;
    cursor_y = old_cursor_y;
}
