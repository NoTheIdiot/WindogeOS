#include <stdint.h>
#include <stddef.h>
#include "vbe.h"
#include "ports.h"
#include "multiboot.h"

// create color
// Create RGB color for current bit depth
uint32_t vbe_make_color(uint8_t r, uint8_t g, uint8_t b) {
    if (vbe_bits_per_pixel == 32) {
        return (r << 16) | (g << 8) | b;
    } else if (vbe_bits_per_pixel == 24) {
        return (r << 16) | (g << 8) | b;
    } else if (vbe_bits_per_pixel == 16) {
        // 565 RGB format
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
    return 0;
}

// some colors since it's VBE, but i won't use them for now
uint32_t black;
uint32_t white;
uint32_t red;
uint32_t green;
uint32_t blue;
uint32_t yellow;
uint32_t cyan;
uint32_t gold;
uint32_t orange;
uint32_t brown;

void init_terminal_colors() {
    black = vbe_make_color(0, 0, 0);
    white = vbe_make_color(255, 255, 255);
    red = vbe_make_color(255, 74, 74);
    green = vbe_make_color(85, 255, 85);
    blue = vbe_make_color(85, 85, 255);
    yellow = vbe_make_color(255, 255, 85);
    cyan = vbe_make_color(85, 255, 255);
    gold = vbe_make_color(255, 215, 0);
    orange = vbe_make_color(230, 149, 59);
    brown = vbe_make_color(212, 163, 115);
}

// Global VBE variables
vbe_mode_info_t vbe_info;
uint32_t vbe_frame_buffer;
uint16_t vbe_pitch;
uint16_t vbe_width = 1024;
uint16_t vbe_height = 768;
uint8_t vbe_bits_per_pixel = 32;
uint8_t vbe_initialized = 0;

// Initialize VBE graphics from multiboot info
void vbe_init_from_multiboot(multiboot_info_t* mbi) {
    // Set safe defaults first
    vbe_width = 640;
    vbe_height = 480;
    vbe_bits_per_pixel = 32;
    vbe_pitch = vbe_width * 4;
    
    if ((mbi->flags & 0x1000) != 0) {
        vbe_frame_buffer = (uint32_t)mbi->framebuffer_addr;
        vbe_pitch = mbi->framebuffer_pitch;
        vbe_width = mbi->framebuffer_width;
        vbe_height = mbi->framebuffer_height;
        vbe_bits_per_pixel = mbi->framebuffer_bpp;
        vbe_initialized = 1;
        return;
    }

    vbe_initialized = 0;
}

void vbe_init_graphics(uint16_t mode __attribute__((unused))) {
    vbe_frame_buffer = vbe_info.frame_buffer;
    vbe_pitch = vbe_info.pitch;
    vbe_width = vbe_info.width;
    vbe_height = vbe_info.height;
    vbe_bits_per_pixel = vbe_info.bits_per_pixel;
}

// Plot a single pixel at (x, y) with given color
void vbe_putpixel(uint16_t x, uint16_t y, uint32_t color) {
    if (x >= vbe_width || y >= vbe_height) {
        return;  // Out of bounds
    }

    uint32_t offset = y * vbe_pitch + x * (vbe_bits_per_pixel / 8);
    uint8_t* frame_buffer = (uint8_t*)vbe_frame_buffer;

    if (vbe_bits_per_pixel == 32) {
        *(uint32_t*)(frame_buffer + offset) = color;
    } else if (vbe_bits_per_pixel == 24) {
        frame_buffer[offset] = color & 0xFF;
        frame_buffer[offset + 1] = (color >> 8) & 0xFF;
        frame_buffer[offset + 2] = (color >> 16) & 0xFF;
    } else if (vbe_bits_per_pixel == 16) {
        *(uint16_t*)(frame_buffer + offset) = color & 0xFFFF;
    }
}

