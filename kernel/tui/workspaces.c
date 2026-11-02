#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <basicutil.h>
#include <string.h>
#include <boot/kernel.h>
#include <boot/scheduler.h>

#define BUFFER_SIZE (TERMINAL_COLS * TERMINAL_ROWS)

typedef struct {
    char display_buffer[BUFFER_SIZE];
    uint32_t text_colors[BUFFER_SIZE];
    uint32_t background_colors[BUFFER_SIZE];
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t text_color;
    uint32_t background_color;
    uint32_t video_cursor_x;
    uint32_t video_cursor_y;
    bool cursor_visible;
    bool video_mode;
} console_t;

static console_t workspaces[MAX_TERMINALS];
static volatile bool switch_requested;
static volatile uint32_t workspace_generation;
static uint32_t rendered_workspace_id = UINT32_MAX;
int current_terminal_id = 0;

static uint64_t workspace_disable_interrupts(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0; cli"
                      : "=r"(flags) : : "memory");
    return flags;
}

static void workspace_restore_interrupts(uint64_t flags) {
    if ((flags & (1ULL << 9)) != 0) {
        sti();
    }
}

void sys_workspace_save(uint32_t workspace_id) {
    if (workspace_id >= MAX_TERMINALS) {
        return;
    }

    console_t *workspace = &workspaces[workspace_id];
    memcpy(workspace->display_buffer, text_grid, sizeof(text_grid));
    memcpy(workspace->text_colors, text_color_grid, sizeof(text_color_grid));
    memcpy(workspace->background_colors, bg_color_grid,
           sizeof(bg_color_grid));
    workspace->cursor_x = cursor_x;
    workspace->cursor_y = cursor_y;
    workspace->text_color = dogeio_color;
    workspace->background_color = dogeio_background_color;
    workspace->video_cursor_x = video_cursor_x;
    workspace->video_cursor_y = video_cursor_y;
    workspace->cursor_visible = dogeio_cursor_visible;
    workspace->video_mode = dogeio_video_mode;
}

static void workspace_load_state(uint32_t workspace_id, bool render) {
    if (workspace_id >= MAX_TERMINALS) {
        return;
    }

    console_t *workspace = &workspaces[workspace_id];
    memcpy(text_grid, workspace->display_buffer, sizeof(text_grid));
    memcpy(text_color_grid, workspace->text_colors, sizeof(text_color_grid));
    memcpy(bg_color_grid, workspace->background_colors,
           sizeof(bg_color_grid));
    cursor_x = workspace->cursor_x;
    cursor_y = workspace->cursor_y;
    dogeio_color = workspace->text_color;
    dogeio_background_color = workspace->background_color;
    video_cursor_x = workspace->video_cursor_x;
    video_cursor_y = workspace->video_cursor_y;
    dogeio_cursor_visible = workspace->cursor_visible;
    dogeio_video_mode = workspace->video_mode;
    dogeio_set_render_enabled(render);
}

void sys_workspace_load(uint32_t workspace_id, bool render) {
    workspace_load_state(workspace_id, render);
    if (render && rendered_workspace_id != workspace_id) {
        dogeio_refresh_screen();
        menubar_draw();
        rendered_workspace_id = workspace_id;
    }
}

void sys_init_terminals(void) {
    for (uint32_t terminal = 0; terminal < MAX_TERMINALS; terminal++) {
        memset(workspaces[terminal].display_buffer, ' ', BUFFER_SIZE);
        for (uint32_t cell = 0; cell < BUFFER_SIZE; cell++) {
            workspaces[terminal].text_colors[cell] = dogeio_color;
            workspaces[terminal].background_colors[cell] =
                dogeio_background_color;
        }
        workspaces[terminal].cursor_x = 0;
        workspaces[terminal].cursor_y = 1;
        workspaces[terminal].text_color = dogeio_color;
        workspaces[terminal].background_color = dogeio_background_color;
        workspaces[terminal].video_cursor_x = 0;
        workspaces[terminal].video_cursor_y = 0;
        workspaces[terminal].cursor_visible = false;
        workspaces[terminal].video_mode = false;
    }
    current_terminal_id = 0;
    switch_requested = false;
    workspace_generation = 0;
    rendered_workspace_id = 0;
    sys_workspace_save(0);
}

void sys_switch_terminal(void) {
    uint64_t flags = workspace_disable_interrupts();
    uint32_t active_workspace = scheduler_current_workspace();
    sys_workspace_save(active_workspace);

    current_terminal_id = (current_terminal_id + 1) % MAX_TERMINALS;
    workspace_generation++;
    dogeio_discard_pending_keys();
    scheduler_set_shell_workspace((uint32_t)current_terminal_id);
    workspace_load_state((uint32_t)current_terminal_id, true);
    dogeio_refresh_screen();
    menubar_draw();
    rendered_workspace_id = (uint32_t)current_terminal_id;
    workspace_restore_interrupts(flags);
}

void sys_workspace_request_switch(void) {
    switch_requested = true;
}

bool sys_workspace_take_switch_request(void) {
    bool requested = switch_requested;
    switch_requested = false;
    return requested;
}

uint32_t sys_workspace_generation(void) {
    return workspace_generation;
}
