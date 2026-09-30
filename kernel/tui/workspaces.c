#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <boot/kernel.h>

#define MAX_TERMINALS 4
#define BUFFER_SIZE (TERMINAL_COLS * TERMINAL_ROWS)

typedef struct {
    char display_buffer[BUFFER_SIZE]; 
} console_t;

static console_t workspaces[MAX_TERMINALS];
int current_terminal_id = 0;

void sys_init_terminals(void) {
    for (int a = 0; a < MAX_TERMINALS; a++) {
        memset(workspaces[a].display_buffer, ' ', BUFFER_SIZE);
    }
}

void sys_switch_terminal(void) {
    for (int j = 0; j < TERMINAL_ROWS; j++) {
        for (int i = 0; i < TERMINAL_COLS; i++) {
            int buffer_index = (j * TERMINAL_COLS) + i;
            workspaces[current_terminal_id].display_buffer[buffer_index] = text_grid[buffer_index];
        }
    }

    if (current_terminal_id == 3) {
        current_terminal_id = 0;
    } else {
        current_terminal_id++;
    }
    
    dogeio_text_clear();

    for (int j = 0; j < TERMINAL_ROWS; j++) {
        for (int i = 0; i < TERMINAL_COLS; i++) {
            int buffer_index = (j * TERMINAL_COLS) + i;
            text_grid[buffer_index] = workspaces[current_terminal_id].display_buffer[buffer_index];
            dogeio_text_putchar(text_grid[buffer_index], (uint32_t)(j), (uint32_t)(i));
        }
    }
    
    menubar_draw();
}
