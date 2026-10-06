#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <boot/kernel.h>

#define BUFFER_SIZE (TERMINAL_COLS * TERMINAL_ROWS)

typedef struct {
    char display_buffer[BUFFER_SIZE];
    uint32_t text_colors[BUFFER_SIZE];
    uint32_t background_colors[BUFFER_SIZE];
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t text_color;
    uint32_t background_color;
} console_t;

static console_t workspaces[MAX_TERMINALS];
int current_terminal_id = 0;

void sys_init_terminals(void) {
    for (uint32_t terminal = 0; terminal < MAX_TERMINALS; terminal++) {
        memset(workspaces[terminal].display_buffer, ' ', BUFFER_SIZE);
        for (uint32_t cell = 0; cell < BUFFER_SIZE; cell++) {
            workspaces[terminal].text_colors[cell] = dogeio_text_color;
            workspaces[terminal].background_colors[cell] =
                dogeio_background_color;
        }
        workspaces[terminal].cursor_x = 0;
        workspaces[terminal].cursor_y = 1;
        workspaces[terminal].text_color = dogeio_text_color;
        workspaces[terminal].background_color = dogeio_background_color;
    }
    current_terminal_id = 0;
}

void sys_switch_terminal(void) {
    console_t *current = &workspaces[current_terminal_id];
    memcpy(current->display_buffer, text_grid, sizeof(text_grid));
    memcpy(current->text_colors, text_color_grid, sizeof(text_color_grid));
    memcpy(current->background_colors, bg_color_grid, sizeof(bg_color_grid));
    current->cursor_x = cursor_x;
    current->cursor_y = cursor_y;
    current->text_color = dogeio_text_color;
    current->background_color = dogeio_background_color;

    current_terminal_id = (current_terminal_id + 1) % MAX_TERMINALS;
    console_t *next = &workspaces[current_terminal_id];

    dogeio_text_clear_raw();
    memcpy(text_grid, next->display_buffer, sizeof(text_grid));
    memcpy(text_color_grid, next->text_colors, sizeof(text_color_grid));
    memcpy(bg_color_grid, next->background_colors, sizeof(bg_color_grid));

    for (uint32_t y = 0; y < TERMINAL_ROWS; y++) {
        for (uint32_t x = 0; x < TERMINAL_COLS; x++) {
            uint32_t cell = y * TERMINAL_COLS + x;
            dogeio_text_color = text_color_grid[cell];
            dogeio_background_color = bg_color_grid[cell];
            dogeio_text_putchar(text_grid[cell], x, y);
        }
    }

    dogeio_text_color = next->text_color;
    dogeio_background_color = next->background_color;
    cursor_x = next->cursor_x;
    cursor_y = next->cursor_y;
    menubar_draw();
}
