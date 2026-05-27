// include files
#include <stdint.h>
#include <stddef.h>
#include "../helper/string.h"
#include "../helper/dogeio.h"

// trying to make a file system
typedef struct {
    char name[32];
    char file_extension[4];
    char content[64][1024];
} vfs_file;

vfs_file readme = {
    .name = "readme",
    .file_extension = "txt",
    .content = {
        "readme.txt",
        "Welcome to WindogeOS!",
        "This is a virtual file system demo.",
        "Files are stored as arrays of strings.",
        "This file system cannot be saved.",
        "This file system is now finally able to be writen!",
        "Enjoy your stay!",
        NULL
    }
};

vfs_file dogescript_example = {
    .name = "dogescript_example",
    .file_extension = "dsc",
    .content = {
        "PRINT HELLO WORLD",
        NULL
    }
};

// array of all files in the virtual file system
vfs_file file_system[16];

// initialize file system
void file_init_filesystem() {
    file_system[0] = readme,
    file_system[1] = dogescript_example,
    NULL
}

// functions
void file_read_file(char* file[]) {
    if (file == NULL || file[0] == NULL) {
        dogeio_print("Error: Invalid file\n");
        return;
    }

    for (int i = 1; file[i] != NULL; i++) {
        dogeio_print(file[i]);
        dogeio_print("\n");
    }
}

void file_list_files() {
    for (int i = 0; file_system[i].name != NULL; i++) {
        dogeio_print(i); // id of a file
        dogeio_println(file_system[i].name);
    }
}

char** file_find_by_name(char* filename) {
    for (int i = 0; virtual_filesystem[i] != NULL; i++) {
        char** file = virtual_filesystem[i];
        if (file[0] != NULL && string_strcmp(file[0], filename) == 0) {
            return file;
        }
    }
    return NULL;
}

void file_rename_file(char* file[], char* new_name) {
    if (file == NULL || file[0] == NULL) {
        dogeio_print("Error: Invalid file that doesn't exist.\n");
        return;
    }

    file[0] = new_name;
}

void file_write_file(char* file[], char* string) {
    if (file == NULL || file[0] == NULL) {
        dogeio_print("Error: Invalid file\n");
        return;
    }

    int num_lines = 0;
    for (int i = 1; file[i] != NULL; i++) {
        num_lines++;
    }

    file[num_lines + 1] = string;
}

void file_delete_line(char* file[], int line) {
    if (file == NULL || file[0] == NULL) {
        dogeio_print("Error: Invalid file\n");
        return;
    }

    int num_lines = 0;
    for (int i = 1; file[i] != NULL; i++) {
        num_lines++;
    }

    if (line == 0 || line > num_lines) {
        dogeio_print("Error: Invalid line number\n");
        return;
    }

    for (int i = line; i < num_lines; i++) {
        file[i] = file[i + 1];
    }
    file[num_lines] = NULL;
}