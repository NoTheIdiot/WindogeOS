#ifndef DOGEIO_H
#define DOGEIO_H

#include <stdint.h>
#include <stddef.h>
#include <bool.h>
#include "core.h"

extern uint8_t terminal_font[128][16];

#define TERMINAL_COLS 160
#define TERMINAL_ROWS 50

extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern uint32_t dogeio_background_color;
extern uint32_t dogeio_text_color;
extern bool dogeio_cursor_visible;
extern char text_grid[TERMINAL_COLS * TERMINAL_ROWS];
extern uint32_t text_color_grid[TERMINAL_ROWS * TERMINAL_COLS];
extern uint32_t bg_color_grid[TERMINAL_ROWS * TERMINAL_COLS];

#define COLOR_BLACK          0x000000
#define COLOR_RED            0xAA0000
#define COLOR_GREEN          0x00AA00
#define COLOR_YELLOW         0xFFFF00
#define COLOR_BLUE           0x0000AA
#define COLOR_MAGENTA        0xAA00AA
#define COLOR_CYAN           0x00AAAA
#define COLOR_WHITE          0xAAAAAA
#define COLOR_DARK_GRAY      0x282828
#define COLOR_ORANGE         0xFFA500

#define COLOR_BRIGHT_BLACK   0x555555
#define COLOR_BRIGHT_RED     0xFF5555
#define COLOR_BRIGHT_GREEN   0x55FF55
#define COLOR_BRIGHT_YELLOW  0xFFED29
#define COLOR_BRIGHT_ORANGE  0xFFA500
#define COLOR_BRIGHT_BLUE    0x5555FF
#define COLOR_BRIGHT_MAGENTA 0xFF55FF
#define COLOR_BRIGHT_CYAN    0x55FFFF
#define COLOR_BRIGHT_WHITE   0xFFFFFF

void dogeio_text_putchar(char c, uint32_t x, uint32_t y);
void dogeio_text_clear(void);
void dogeio_text_printchar(char c);
void dogeio_text_print(const char *str);
void dogeio_text_println(const char *str);
void dogeio_text_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color);
void dogeio_text_input(const char* prompt, char* buffer, size_t max_str_length);
uint16_t dogeio_get_key(void);
void dogeio_text_color_change(uint32_t color);
void dogeio_text_background_change(uint32_t color);
void dogeio_text_clear_raw(void);
void dogeio_print_hex8(uint8_t val);
void dogeio_text_cursor_show();
void dogeio_text_cursor_hide();
void dogeio_print_hex16(uint16_t val);

#define KEY_UP        0x101
#define KEY_DOWN      0x102
#define KEY_LEFT      0x103
#define KEY_RIGHT     0x104
#define KEY_BACKSPACE 0x008
#define KEY_ENTER     0x00A
#define KEY_TAB       0x009
#define KEY_UNKNOWN   0x000
#define KEY_F1        0x105
#define KEY_F2        0x106
#define KEY_CTRL_O    0x110
#define KEY_CTRL_N    0x111

#define FS_PERM_READ   0x01u
#define FS_PERM_WRITE  0x02u
#define FS_PERM_EXEC   0x04u
#define FS_PERM_DELETE 0x08u
#define FS_PERM_CREATE 0x10u
#define FS_PERM_LIST   0x20u

typedef struct {
    char path[128];
    uint32_t owner_uid;
    uint32_t group_gid;
    uint32_t owner_mask;
    uint32_t group_mask;
    uint32_t other_mask;
} fs_acl_t;

typedef struct {
    uint64_t size;
    uint64_t is_dir;
    uint64_t exists;
} dogec_stat_t;

int fs_set_auth_override(int enabled);
int fs_set_permissions(const char *path, uint32_t owner_uid, uint32_t group_gid,
                       uint32_t owner_mask, uint32_t group_mask, uint32_t other_mask);
int fs_check_access(const char *path, uint32_t requested_mask);

int   fs_format(void);
int   fs_create(char* filename);
int   fs_mkdir(char* foldername);
int   fs_exists(char* filename);
int   fs_delete(char* filename);
int   fs_delete_last_line(char* filename);
int   fs_read(char* filename, char* output_buffer, uint64_t max_size);
int   fs_write(char* filename, char* input_buffer);
int   fs_append_data(char* filename, const uint8_t* input_buffer, uint32_t size);
int   fs_list_dir(int hidden);
int   fs_rename(char* filename, char* newname);
int   fs_copy(char* source, char* dest);
int   fs_move(char* source, char* dest);
int   fs_chdir(char* folder);
char* fs_dirname(void);
int   fs_mount(void);
int   fs_list_files(const char *directory, char (*names)[256], size_t capacity);
int   fs_list(const char* directory, int show_hidden);
int   fs_read_raw(char* filename, uint8_t* output_buffer, uint64_t max_size);
int   fs_read_raw_at(char* filename, uint8_t* output_buffer, uint64_t offset, uint64_t max_size);

int exec_flat_binary(const char *filename, int argc, char **argv);

#endif
