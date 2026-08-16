// this is to parse .tga files
#include <system.h>
#include <boot/kernel.h>
#include <stdint.h>
#include <stddef.h> 
#include <dogeio.h>

int system_parse_tga(char* filename) {
    // allocate 18.75KB of ram for images
    // and output 80x80 image which is
    volatile char image_buffer[19200];
    volatile uint32_t rendered_image[80][80];

    if (!fs_exists(filename)) {
        return 0;
    }

    fs_read(filename, (char*)image_buffer, 19200);

    for (int i = 0; i < 6400; i++) {
        int buffer_index = i * 3;

        uint8_t r = (uint8_t)image_buffer[buffer_index];
        uint8_t g = (uint8_t)image_buffer[buffer_index + 1];
        uint8_t b = (uint8_t)image_buffer[buffer_index + 2];

        uint32_t color = rgb_to_u32(r, g, b);

        int img_x = i % 80;
        int img_y = i / 80;

        rendered_image[img_y][img_x] = color;
    }

    uint64_t offset = 400;
    dogeio_text_clear();

    for (int x = 0; x < 80; x++) {
        for (int y = 0; y < 80; y++) {
            uint32_t final_color = rendered_image[y][x];
            put_pixel(offset + (uint64_t)x, offset + (uint64_t)y, final_color); 
        }
    }

    char finish[2];
    dogeio_text_input("press enter to exit", finish, 2);

    dogeio_text_clear();
    return 1;
}
