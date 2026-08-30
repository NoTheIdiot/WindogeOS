// this is to parse .tga files
#include <system.h>
#include <boot/kernel.h>
#include <boot/limine.h>
#include <stdint.h>
#include <stddef.h> 
#include <image.h>
#include <dogeio.h>

extern volatile struct limine_framebuffer_request framebuffer_request;

void put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    if (x >= fb->width || y >= fb->height) {
        return;
    }

    uint32_t *fb_ptr = (uint32_t *)fb->address;
    uint64_t pitch_pixels = fb->pitch / 4;

    fb_ptr[y * pitch_pixels + x] = color;
}


int system_parse_tga(char* filename) {
    static uint8_t image_buffer[20000];
    uint32_t rendered_image[80][80];

    if (!fs_exists(filename)) {
        return 0;
    }

    fs_read(filename, (char*)image_buffer, sizeof(image_buffer));

    tga_header_t* header = (tga_header_t*)image_buffer;

    uint8_t* pixel_data = image_buffer + 18 + header->id_length;
    int top_down = (header->image_descriptor & 0x20) != 0;

    for (int i = 0; i < 6400; i++) {
        int buffer_index = i * 3;

        uint8_t b = pixel_data[buffer_index];
        uint8_t g = pixel_data[buffer_index + 1];
        uint8_t r = pixel_data[buffer_index + 2];

        uint32_t color = rgb_to_u32(r, g, b);

        int img_x = i % 80;
        int raw_y = i / 80;

        int img_y = top_down ? raw_y : (79 - raw_y);

        rendered_image[img_y][img_x] = color;
    }

    uint64_t offset = 400;
    dogeio_text_clear();

    for (int y = 0; y < 80; y++) {
        for (int x = 0; x < 80; x++) {
            uint32_t final_color = rendered_image[y][x];
            put_pixel(offset + (uint64_t)x, offset + (uint64_t)y, final_color); 
        }
    }

    char finish[2];
    dogeio_text_input("press enter to exit", finish, 2);

    dogeio_text_clear();
    return 1;
}

// make a random image that i just one solid color
void generate_tga(char* filename, uint8_t r, uint8_t g, uint8_t b) {
    static uint8_t file_buffer[19218];

    if (fs_exists(filename)) {
        fs_delete(filename);
    }
    fs_create(filename);

    tga_header_t* header = (tga_header_t*)file_buffer;
    header->id_length = 0;
    header->color_map_type = 0;
    header->image_type = 2; 
    header->color_map_origin = 0;
    header->color_map_length = 0;
    header->color_map_depth = 0;
    header->x_origin = 0;
    header->y_origin = 0;
    header->width = 80;
    header->height = 80;
    header->bits_per_pixel = 24;
    header->image_descriptor = 0x20;

    uint8_t* pixels = file_buffer + 18;
    for (int i = 0; i < 6400; i++) {
        pixels[i * 3 + 0] = b;
        pixels[i * 3 + 1] = g;
        pixels[i * 3 + 2] = r;
    }

    exfat_write_file(filename, file_buffer, sizeof(file_buffer));
}