// include files
#include <stdint.h>
#include <stddef.h>
#include "../helper/string.h"
#include "../helper/dogeio.h"

// Virtual file definitions
char* file1[] = {
    "readme.txt",
    "Welcome to WindogeOS!",
    "This is a virtual file system demo.",
    "Files are stored as arrays of strings.",
    "This file system cannot be saved.",
    "This file system also is read only.",
    "Enjor your stay!",
    NULL
};

char* file2[] = {
    "config.sys",
    "kernel=windoge",
    "shell=dogeshell",
    "graphics=vbe",
    NULL
};

char* file3[] = {
    "hello.doge",
    "PRINT \"HELLO WORLD\"",
    NULL
};

// array of all files in the virtual file system
char** virtual_filesystem[] = {
    file1,
    file2,
    file3,
    NULL
};

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
    for (int i = 0; virtual_filesystem[i] != NULL; i++) {
        char** file = virtual_filesystem[i];
        if (file[0] != NULL) {
            dogeio_print("- ");
            dogeio_print(file[0]);
            dogeio_print("\n");
        }
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