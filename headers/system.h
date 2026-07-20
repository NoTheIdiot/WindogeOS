#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <bool.h>
#include <stddef.h>

int system_dogeshell_ex(char* command);
void system_dogeshell();

#define FS_CONTENT_ROWS 256
#define FS_CONTENT_COLS 512
#define FS_CONTENT_MAX (FS_CONTENT_COLS * FS_CONTENT_ROWS)
#define FS_MAX_FILE_AMOUNT 32

/* 
|------------------- ROWS
|
| COLS
|
*/

typedef struct {
	char name[32];
	char ext [4];
	char content[FS_CONTENT_COLS][FS_CONTENT_ROWS];
	uint8_t cols_last;
} file_t;

file_t* system_fs_find(char* filename);
void system_fs_format();
void system_fs_list();
void system_fs_createfile(char* name, char* ext);
bool system_fs_readfile(char* name);
void system_fs_start_init();

#endif