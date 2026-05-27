#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002
#define MBI_FLAG_MMAP               (1 << 6)
#define MBI_FLAG_FRAMEBUFFER        (1 << 12)

// VBE/Framebuffer info structure
typedef struct {
    uint32_t frame_buffer_addr;
    uint32_t frame_buffer_pitch;
    uint32_t frame_buffer_width;
    uint32_t frame_buffer_height;
    uint8_t frame_buffer_bpp;
    uint8_t frame_buffer_type;
    union {
        struct {
            uint32_t frame_buffer_palette_addr;
            uint16_t frame_buffer_palette_num_colors;
        } palette;
        struct {
            uint8_t frame_buffer_red_field_position;
            uint8_t frame_buffer_red_mask_size;
            uint8_t frame_buffer_green_field_position;
            uint8_t frame_buffer_green_mask_size;
            uint8_t frame_buffer_blue_field_position;
            uint8_t frame_buffer_blue_mask_size;
        } rgb;
    } color_info;
} vbe_framebuffer_info_t;

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[24];
} multiboot_info_t;

struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

#endif