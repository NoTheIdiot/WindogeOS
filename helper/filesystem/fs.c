#include <string.h>
#include <system.h>
#include <dogeio.h>
#include <bool.h>
#include <stddef.h>

file_t filesystem[FS_MAX_FILE_AMOUNT];

file_t* system_fs_find(char* filename, char* ext) {
	for (int i = 0; i < FS_MAX_FILE_AMOUNT; i++) {
		if (filesystem[i].name[0] != '\0') {
			if (string_strcmp(filesystem[i].name, filename) == 0 || string_strcmp(filesystem[i].ext, ext) == 0) {
				return &filesystem[i];
			}
		}
	}
	return NULL;
}

void system_fs_format() {
	for (int i = 0; i < FS_MAX_FILE_AMOUNT; i++) {
		filesystem[i].name[0] = '\0';
		filesystem[i].cols_last = 0;
		filesystem[i].ext[0] = '\0';
		filesystem[i].content[0][0] = '\0';
	}
}

void system_fs_list() {
	dogeio_text_println("name");
	dogeio_text_println("-------------------------");
	for (int i = 0; i < FS_MAX_FILE_AMOUNT; i++) {
		if (filesystem[i].name[0] == '\0') {
			continue;
		}
		dogeio_text_print(filesystem[i].name);
		dogeio_text_print(".");
		dogeio_text_println(filesystem[i].ext);
	}
}

void system_fs_createfile(char* name, char* ext) {
	int target_index = -1;
	for (int i = 0; i < FS_MAX_FILE_AMOUNT; i++) {
		if (filesystem[i].name[0] == '\0') {
			target_index = i;
			break;
		}
	}

	if (target_index == -1) {
		dogeio_text_println("Your drive is full, please delete some useless files");
		return;
	}

	if (system_fs_find(name, ext) != NULL) {
		dogeio_text_println("Error: A file with that name already exists.");
		return;
	}

	int j;
	for (j = 0; j < 31 && name[j] != '\0'; j++) {
		filesystem[target_index].name[j] = name[j];
	}
	filesystem[target_index].name[j] = '\0';

	for (j = 0; j < 3 && ext[j] != '\0'; j++) {
		filesystem[target_index].ext[j] = ext[j];
	}
	filesystem[target_index].ext[j] = '\0';

	filesystem[target_index].cols_last = 0;
	filesystem[target_index].content[0][0] = '\0';
}

bool system_fs_readfile(char* name, char* ext) {
	file_t* file = system_fs_find(name, ext);
	
	if (file == NULL) {
		dogeio_text_println("Error: File does not exist.");
		return false;
	} else {
		for (int i = 0; i < file->cols_last; i++) {
			dogeio_text_println(file->content[i]);
		}
		return true;
	}
}

bool system_fs_writefile(char* name, char* ext, char* string) {
	file_t* file = system_fs_find(name, ext);
	
	if (file == NULL) {
		dogeio_text_println("Error: File does not exist.");
		return false;
	}

	if ((uint32_t)file->cols_last >= (uint32_t)FS_CONTENT_COLS) {
		dogeio_text_println("Error: File is full.");
		return false;
	}

	string_strcpy(file->content[file->cols_last], string);
	
	file->cols_last++;
	return true;
}

bool system_fs_deleteline(char* filename, char* ext) {
	file_t* file = system_fs_find(filename, ext);
	
	if (file == NULL) {
		dogeio_text_println("Error: File does not exist.");
		return false;
	}

	if (file->cols_last == 0) {
		dogeio_text_println("Error: File is already empty.");
		return false;
	}

	file->cols_last--;
	file->content[file->cols_last][0] = '\0';
	return true;
}

/*bool system_fs_deletefile(char* filename, char* ext) {
	file_t* file = system_fs_find(filename, ext);
	if (file == NULL) {

	}
}*/

void system_fs_start_init() {
	system_fs_format();
	system_fs_createfile("readme", "txt"); 
	system_fs_writefile("readme", "txt", "Welcome to WindogeOS v0.0.3! Do anything around here");
}
