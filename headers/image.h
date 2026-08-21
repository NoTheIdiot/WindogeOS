#ifndef IMAGE_H
#define IMAGE_H

int system_parse_tga(char* filename);
typedef struct __attribute__((packed)) {
    uint8_t  id_length;
    uint8_t  color_map_type;
    uint8_t  image_type;
    uint16_t color_map_origin;
    uint16_t color_map_length;
    uint8_t  color_map_depth;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t  bits_per_pixel;
    uint8_t  image_descriptor;
} tga_header_t;

#endif
