#include <string.h>
#include <system.h>
#include <dogeio.h>
#include <bool.h>
#include <stddef.h>

int fileamount;
file_t filesystem[FS_MAX_FILE_AMOUNT];

file_t* system_fs_find(char* filename) {
	for (int i = 0; i < FS_MAX_FILE_AMOUNT; i++) {
		if (string_strcmp(filesystem[i].name, filename) == 0) {
			return &filesystem[i];
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
	fileamount = 0;
}

void system_fs_list() {
	for (int i = 0; i < FS_MAX_FILE_AMOUNT; i++) {
		if (filesystem[i].name[0] == '\0') {
            continue;
        }

		dogeio_text_println("name");
		dogeio_text_println("-------------------------");
		dogeio_text_print(filesystem[i].name);
		dogeio_text_print(".");
		dogeio_text_println(filesystem[i].ext);
	}
}

void system_fs_createfile(char* name, char* ext) {
	if (fileamount >= FS_MAX_FILE_AMOUNT) {
		dogeio_text_println("Your drive is full, please delete some useless files");
		return;
	}

	for (int j = 0; j < 31; j++) {
        filesystem[fileamount].name[j] = name[j];
        if (name[j] == '\0') break;
	}
	filesystem[fileamount].name[31] = '\0';

	for (int j = 0; j < 3; j++) {
        filesystem[fileamount].ext[j] = ext[j];
        if (ext[j] == '\0') break;
    }
    filesystem[fileamount].ext[3] = '\0';

	filesystem[fileamount].cols_last = 0;
    filesystem[fileamount].content[0][0] = '\0';

	fileamount++;
}

bool system_fs_readfile(char* name) {
	file_t* file = system_fs_find(name);
	
	if (file == NULL) {
		dogeio_text_println("Error: File does not exist.");
		return false;
	} else {
		for (int i = 0; i < FS_CONTENT_COLS; i++) {
			if (file->content[i][0] == '\0') {
				break; 
			}

			dogeio_text_println(file->content[i]);
		}
		return true;
	}
}

bool system_fs_writefile(char* name, char* string) {
	file_t* file = system_fs_find(name);
	
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

void system_fs_start_init() {
	system_fs_format();
	system_fs_createfile("readme", "txt"); 
	system_fs_writefile("readme", "Welcome to WindogeOS v0.0.3! Do anything around here");
}
