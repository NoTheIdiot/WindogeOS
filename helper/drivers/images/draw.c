// this file is for drawing pixels to the limine framebuffer
#include <boot/limine.h>
#include <boot/kernel.h>
#include <dogeio.h>
#include <stdint.h>
#include <basicutil.h>
#include <bool.h>

// get the framebuffer
extern volatile struct limine_framebuffer_request framebuffer_request;

// why am i going to add image rendering? because it's cool ig but idk
// how am i going to do this in the shell
[[maybe_unused]] void put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        log("Draw Pixel: dumbass the framebuffrer doesn't exist");
        return;
    }

    // get the framebuffer
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    // prevent out of bounds
    if (x >= fb->width || y >= fb->height) {
        log("Draw Pixel: out of bounds dumbass");
        return;
    }

    volatile uint32_t *pixel_address = (volatile uint32_t*)((uint8_t*)fb->address + (y * fb->pitch) + (x * 4));
    *pixel_address = color;
}