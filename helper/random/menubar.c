#include <stdint.h>
#include <dogeio.h>
#include <boot/kernel.h>
#include <time.h>
#include <system.h>

#define TERMINAL_COLS 160

void str_pad(char *dest, const char *src, int target_len, char pad_char);

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

    char* current_time = time_get();
    int time_len = 0;
    while (current_time[time_len] != '\0') {
        time_len++;
    }

    int left_section_target_len = TERMINAL_COLS - time_len - 1;

    dogeio_text_print(" ");
    str_pad(menu_buffer, windoge_version, left_section_target_len - 1, ' ');

    dogeio_text_print(menu_buffer);
    dogeio_text_print(current_time);
    dogeio_text_print(" ");

    dogeio_text_color_change(old_text_color);
    dogeio_text_background_change(old_bg_color);
    cursor_x = old_cursor_x;
    cursor_y = old_cursor_y;
}
