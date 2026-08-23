#ifndef DOGEIO_H
#define DOGEIO_H

#include <stdint.h>
#include <stddef.h>

extern uint8_t terminal_font[128][16];

#define TERMINAL_COLS 160
#define TERMINAL_ROWS 50

extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern uint32_t dogeio_background_color;
extern uint32_t dogeio_text_color;

#define COLOR_BLACK          0x000000
#define COLOR_RED            0xAA0000
#define COLOR_GREEN          0x00AA00
#define COLOR_YELLOW         0xAA5500
#define COLOR_BLUE           0x0000AA
#define COLOR_MAGENTA        0xAA00AA
#define COLOR_CYAN           0x00AAAA
#define COLOR_WHITE          0xAAAAAA

#define COLOR_BRIGHT_BLACK   0x555555
#define COLOR_BRIGHT_RED     0xFF5555
#define COLOR_BRIGHT_GREEN   0x55FF55
#define COLOR_BRIGHT_YELLOW  0xFFFF55
#define COLOR_BRIGHT_BLUE    0x5555FF
#define COLOR_BRIGHT_MAGENTA 0xFF55FF
#define COLOR_BRIGHT_CYAN    0x55FFFF
#define COLOR_BRIGHT_WHITE   0xFFFFFF

#ifdef WINDOGE_APP

// who made the C compiler only use 32 bit call
// i will find you.
void _k_dogeio_text_putchar(char c, uint32_t x, uint32_t y) __asm__("dogeio_text_putchar");
void _k_dogeio_text_clear(void) __asm__("dogeio_text_clear");
void _k_dogeio_text_printchar(char c) __asm__("dogeio_text_printchar");
void _k_dogeio_text_print(const char *str) __asm__("dogeio_text_print");
void _k_dogeio_text_println(const char *str) __asm__("dogeio_text_println");
void _k_dogeio_text_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color) __asm__("dogeio_text_print_at");
void _k_dogeio_text_input(const char* prompt, char* buffer, size_t max_str_length) __asm__("dogeio_text_input");
void _k_dogeio_text_color_change(uint32_t color) __asm__("dogeio_text_color_change");
void _k_dogeio_text_background_change(uint32_t color) __asm__("dogeio_text_background_change");
void _k_dogeio_text_clear_raw(void) __asm__("dogeio_text_clear_raw");

int   _k_fs_format(void) __asm__("fs_format");
int   _k_fs_create(char* filename) __asm__("fs_create");
int   _k_fs_mkdir(char* foldername) __asm__("fs_mkdir");
int   _k_fs_exists(char* filename) __asm__("fs_exists");
int   _k_fs_delete(char* filename) __asm__("fs_delete");
int   _k_fs_delete_last_line(char* filename) __asm__("fs_delete_last_line");
int   _k_fs_read(char* filename, char* output_buffer, uint32_t max_size) __asm__("fs_read");
int   _k_fs_write(char* filename, char* input_buffer) __asm__("fs_write");
int   _k_fs_list_dir(int hidden) __asm__("fs_list_dir");
int   _k_fs_rename(char* filename, char* newname) __asm__("fs_rename");
void  _k_fs_copy(char* source, char* dest) __asm__("fs_copy");
int   _k_fs_chdir(char* folder) __asm__("fs_chdir");
char* _k_fs_dirname(void) __asm__("fs_dirname");
int   _k_fs_mount(void) __asm__("fs_mount");

int   _k_exec_flat_binary(const char *filename, int argc, char **argv) __asm__("exec_flat_binary");

// 64-bit indirect call wrappers (movabs rax, addr; call rax)
#define dogeio_text_putchar(c, x, y) (((void(* volatile)(char, uint32_t, uint32_t))&_k_dogeio_text_putchar)(c, x, y))
#define dogeio_text_clear() (((void(* volatile)(void))&_k_dogeio_text_clear)())
#define dogeio_text_printchar(c) (((void(* volatile)(char))&_k_dogeio_text_printchar)(c))
#define dogeio_text_print(str) (((void(* volatile)(const char*))&_k_dogeio_text_print)(str))
#define dogeio_text_println(str) (((void(* volatile)(const char*))&_k_dogeio_text_println)(str))
#define dogeio_text_print_at(str, x, y, col) (((void(* volatile)(const char*, uint32_t, uint32_t, uint32_t))&_k_dogeio_text_print_at)(str, x, y, col))
#define dogeio_text_input(prompt, buf, len) (((void(* volatile)(const char*, char*, size_t))&_k_dogeio_text_input)(prompt, buf, len))
#define dogeio_text_color_change(color) (((void(* volatile)(uint32_t))&_k_dogeio_text_color_change)(color))
#define dogeio_text_background_change(color) (((void(* volatile)(uint32_t))&_k_dogeio_text_background_change)(color))
#define dogeio_text_clear_raw() (((void(* volatile)(void))&_k_dogeio_text_clear_raw)())

#define fs_format() (((int(* volatile)(void))&_k_fs_format)())
#define fs_create(file) (((int(* volatile)(char*))&_k_fs_create)(file))
#define fs_mkdir(dir) (((int(* volatile)(char*))&_k_fs_mkdir)(dir))
#define fs_exists(file) (((int(* volatile)(char*))&_k_fs_exists)(file))
#define fs_delete(file) (((int(* volatile)(char*))&_k_fs_delete)(file))
#define fs_delete_last_line(file) (((int(* volatile)(char*))&_k_fs_delete_last_line)(file))
#define fs_read(file, buf, sz) (((int(* volatile)(char*, char*, uint32_t))&_k_fs_read)(file, buf, sz))
#define fs_write(file, buf) (((int(* volatile)(char*, char*))&_k_fs_write)(file, buf))
#define fs_list_dir(h) (((int(* volatile)(int))&_k_fs_list_dir)(h))
#define fs_rename(old, new) (((int(* volatile)(char*, char*))&_k_fs_rename)(old, new))
#define fs_copy(src, dst) (((void(* volatile)(char*, char*))&_k_fs_copy)(src, dst))
#define fs_chdir(dir) (((int(* volatile)(char*))&_k_fs_chdir)(dir))
#define fs_dirname() (((char*(* volatile)(void))&_k_fs_dirname)())
#define fs_mount() (((int(* volatile)(void))&_k_fs_mount)())

#define exec_flat_binary(file, ac, av) (((int(* volatile)(const char*, int, char**))&_k_exec_flat_binary)(file, ac, av))

#else

// Kernel build prototypes
void dogeio_text_putchar(char c, uint32_t x, uint32_t y);
void dogeio_text_clear(void);
void dogeio_text_printchar(char c);
void dogeio_text_print(const char *str);
void dogeio_text_println(const char *str);
void dogeio_text_print_at(const char *str, uint32_t x_pos, uint32_t y_pos, uint32_t text_color);
void dogeio_text_input(const char* prompt, char* buffer, size_t max_str_length);
void dogeio_text_color_change(uint32_t color);
void dogeio_text_background_change(uint32_t color);
void dogeio_text_clear_raw(void);

int   fs_format(void);
int   fs_create(char* filename);
int   fs_mkdir(char* foldername);
int   fs_exists(char* filename);
int   fs_delete(char* filename);
int   fs_delete_last_line(char* filename);
int   fs_read(char* filename, char* output_buffer, uint32_t max_size);
int   fs_write(char* filename, char* input_buffer);
int   fs_list_dir(int hidden);
int   fs_rename(char* filename, char* newname);
void  fs_copy(char* source, char* dest);
int   fs_chdir(char* folder);
char* fs_dirname(void);
int   fs_mount(void);

int exec_flat_binary(const char *filename, int argc, char **argv);

#endif

#endif