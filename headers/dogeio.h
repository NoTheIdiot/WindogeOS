// on god that this runs on old ass hardware smoothly
// for bragging rights lolololoolololol
#ifndef DOGEIO_H
#define DOGEIO_H

#include <stdint.h>
#include <stddef.h>

// colors cuz why not
#define COLOR_BLACK     0xFF000000
#define COLOR_WHITE     0xFFFFFFFF
#define COLOR_GREY      0xFF808080
#define COLOR_DARK_GREY 0xFF404040

#define COLOR_RED       0xFFFF0000
#define COLOR_GREEN     0xFF00FF00
#define COLOR_BLUE      0xFF0000FF
#define COLOR_YELLOW    0xFFFFFF00
#define COLOR_CYAN      0xFF00FFFF
#define COLOR_MAGENTA   0xFFFF00FF

#define COLOR_NAVY      0xFF000080
#define COLOR_MAROON    0xFF800000
#define COLOR_TEAL      0xFF008080
#define COLOR_OLIVE     0xFF808000

// wow doge colors
#define COLOR_DOGE_GOLD 0xFFE1B857  
#define COLOR_DOGE_TAN  0xFFF4DFB1  


extern const uint8_t terminal_font[256][16];
void dogeio_text_putchar(char c, uint32_t x_pos, uint32_t y_pos, int clear_cell);
void dogeio_text_printchar(char c);
void dogeio_text_hide_cursor(void);
void dogeio_text_redraw_cursor(void);
void dogeio_text_update_cursor(void);
void dogeio_text_print(const char *str);
void dogeio_text_println(const char* str);
void dogeio_text_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color);
void dogeio_text_clear();
void dogeio_text_change_color(uint32_t new_hex_color);

// inputs
char dogeio_text_getchar(void);
void dogeio_text_input(const char *prompt, char *buffer, size_t max_len);

// serial stuff i guess wtf
void dogeio_text_serial_init();
int dogeio_text_serial_transmit_empty();
void dogeio_text_serial_putchar(char c);
void dogeio_text_serial_print(const char* str);
void dogeio_text_serial_println(const char* str);

// basic
void dogeio_log(const char* string);
void dogeio_duolog(const char* string);

#endif