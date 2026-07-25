#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <bool.h>
#include <stddef.h>

int system_dogeshell_ex(char* command);
void system_dogeshell();
void system_bash();

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
	char ext [8];		// ill use .file
	char content[FS_CONTENT_COLS][FS_CONTENT_ROWS];
	uint8_t cols_last;
} file_t;

file_t* system_fs_find(char* filename, char* ext);
void system_fs_format();
void system_fs_list();
void system_fs_createfile(char* name, char* ext);
bool system_fs_readfile(char* name, char* ext);
bool system_fs_writefile(char* name, char* ext, char* string);
void system_fs_start_init();
int system_get_file_amount();
char* system_file_amount_string();

void system_fetch();
extern char* windoge_version;

#endif