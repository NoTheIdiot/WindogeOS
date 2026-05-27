#ifndef VBE_H
#define VBE_H

#include <stdint.h>
#include "multiboot.h"

// VBE info structures
typedef struct {
    uint16_t attributes;
    uint8_t window_a;
    uint8_t window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t char_width;
    uint8_t char_height;
    uint8_t planes;
    uint8_t bits_per_pixel;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved;
    
    // VBE 2.0 and above
    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;
    
    uint32_t frame_buffer;
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;
    uint8_t reserved2[206];
} vbe_mode_info_t;

// Global VBE variables
extern vbe_mode_info_t vbe_info;
extern uint32_t vbe_frame_buffer;
extern uint16_t vbe_pitch;
extern uint16_t vbe_width;
extern uint16_t vbe_height;
extern uint8_t vbe_bits_per_pixel;
extern uint8_t vbe_initialized;

// Function declarations
void vbe_init_from_multiboot(multiboot_info_t* mbi);
void vbe_init_graphics(uint16_t mode);
void vbe_putpixel(uint16_t x, uint16_t y, uint32_t color);
uint32_t vbe_make_color(uint8_t r, uint8_t g, uint8_t b);

#endif
