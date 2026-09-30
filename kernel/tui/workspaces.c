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
    memcpy(workspaces[current_terminal_id].display_buffer, text_grid, BUFFER_SIZE);

    if (current_terminal_id == 3) {
        current_terminal_id = 0;
    } else {
        current_terminal_id++;
    }

    for (int i = 0; i < TERMINAL_COLS; i++) {
        for (int j = 0; j < TERMINAL_ROWS; j++) {
            int buffer_index = (j * TERMINAL_COLS) + i;
            dogeio_text_putchar(workspaces[current_terminal_id].display_buffer[buffer_index], (uint32_t)(j), (uint32_t)(i));
        }
    }
    
    menubar_draw();
}
