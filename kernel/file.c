// include files
#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "dogeio.h"

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
vfs_file file_system[16] {NULL};

// initialize file system
void file_init_filesystem() {
    file_system[0] = &readme,
    file_system[1] = &dogescript_example,
    NULL
}

// find a file by it's name
vfs_file* file_find_by_name(const char* filename) {
    for (int i = 0; i < 16; i++) {
        if (file_system[i] == NULL) continue; 
        
        if (string_strcmp(file_system[i]->name, filename) == 0) {
            return file_system[i];
        }
    }
    return NULL; 
}

// functions
void file_read_file(const char* file) {
    vfs_file file_var = file_find_by_name(file);
    if (file_var == NULL) {
        dogeio_println("File isn't found. Pls try using much [dir]?");
    } else {
        for (int i = 0; file_var->content[i] != NULL; i++) {
            dogeio_println(file_var->content[i]);
        }
    }
}

void file_list_files() {
    dogeio_print("ID");
    dogeio_println("    FILES");
    dogeio_println("-----------");
    for (int i = 0; file_system[i]->name != NULL; i++) {
        dogeio_print(i); // id of a file
        dogeio_print("    ");
        dogeio_println(file_system[i]->name);
    }
}

void file_rename_file(char* filename, char* new_name) {
    vfs_file file_var = file_find_by_name(filename);
    if (file_var == NULL) { dogeio_println("File not found, not wow. Try using many [dir]?"); return; }
    file_var -> name = new_name;
}

void file_write_file(char* filename, char* string) {
    vfs_file file_var = file_find_by_name(filename);
    if (file_var == NULL) { dogeio_println("File not found, not wow. Try using many [dir]?"); return; }
    file_var -> content[]
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