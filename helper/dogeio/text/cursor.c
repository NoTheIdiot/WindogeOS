#include <dogeio.h>
#include <limine.h>

extern volatile struct limine_framebuffer_request framebuffer_request;
extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern uint32_t dogeio_text_bcolor;
extern uint32_t dogeio_text_tcolor;

static int cursor_drawn = 0;
static uint32_t cursor_prev_x = 0;
static uint32_t cursor_prev_y = 0;
static uint32_t cursor_saved_pixels[8];

static void dogeio_text_save_cursor_row(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = (uint32_t *)fb->address;

    uint32_t line_y = cursor_y + 8;
    if (line_y >= fb->height) {
        return;
    }

    for (int col = 0; col < 8; col++) {
        if (cursor_x + col >= fb->width) {
            cursor_saved_pixels[col] = 0;
            continue;
        }

        size_t pixel_index = line_y * (fb->pitch / 4) + (cursor_x + col);
        cursor_saved_pixels[col] = fb_ptr[pixel_index];
    }
}

static void dogeio_text_restore_cursor_row(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = (uint32_t *)fb->address;

    uint32_t line_y = cursor_prev_y + 8;
    if (line_y >= fb->height) {
        return;
    }

    for (int col = 0; col < 8; col++) {
        if (cursor_prev_x + col >= fb->width) {
            break;
        }

        size_t pixel_index = line_y * (fb->pitch / 4) + (cursor_prev_x + col);
        fb_ptr[pixel_index] = cursor_saved_pixels[col];
    }
}

static void dogeio_text_draw_cursor(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    dogeio_text_save_cursor_row();
    uint32_t saved_color = dogeio_text_tcolor;
    dogeio_text_tcolor = COLOR_WHITE;
    dogeio_text_putchar('_', cursor_x, cursor_y, 0);
    dogeio_text_tcolor = saved_color;
}

static void dogeio_text_wrap_cursor(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    if (cursor_x + 8 > fb->width) {
        cursor_x = 0;
        cursor_y += 16;
    }

    if (cursor_y + 16 > fb->height) {
        dogeio_text_clear();
    }
}

void dogeio_text_hide_cursor(void) {
    if (cursor_drawn) {
        dogeio_text_restore_cursor_row();
        cursor_drawn = 0;
    }
}

void dogeio_text_redraw_cursor(void) {
    if (cursor_drawn) {
        return;
    }

    dogeio_text_wrap_cursor();
    dogeio_text_draw_cursor();
    cursor_prev_x = cursor_x;
    cursor_prev_y = cursor_y;
    cursor_drawn = 1;
}

void dogeio_text_update_cursor() {
    dogeio_text_hide_cursor();
    dogeio_text_wrap_cursor();
    dogeio_text_draw_cursor();

    cursor_prev_x = cursor_x;
    cursor_prev_y = cursor_y;
    cursor_drawn = 1;
}
