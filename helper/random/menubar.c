#include <stdint.h>
#include <dogeio.h>
#include <boot/kernel.h>
#include <time.h>

#define TERMINAL_COLS 160

void menubar_draw() {
    uint32_t old_text_color = dogeio_text_color;
    uint32_t old_bg_color = dogeio_background_color;
    uint32_t old_cursor_x = cursor_x;
    uint32_t old_cursor_y = cursor_y;

    dogeio_text_color_change(0x000000);
    dogeio_text_background_change(0xffffff);
    
    cursor_x = 0;
    cursor_y = 0;

    dogeio_text_print(" WindogeOS v0.0.3");

    char* current_time = time_get();
    int time_len = 0;
    while (current_time[time_len] != '\0') {
        time_len++;
    }

    int title_len = 17;
    int padding = TERMINAL_COLS - title_len - time_len - 1;

    for (int i = 0; i < padding; i++) {
        dogeio_text_print(" ");
    }

    dogeio_text_print(current_time);
    dogeio_text_print(" ");

    dogeio_text_color_change(old_text_color);
    dogeio_text_background_change(old_bg_color);
    cursor_x = old_cursor_x;
    cursor_y = old_cursor_y;
}
