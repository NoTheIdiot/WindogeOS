#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "dogeio.h"
#include "consts.h"

#define MAX_FILES_AMOUNT 64

size_t file_amount;

typedef struct {
    char name[32];
    char file_extension[8];
    char content[64][1024];
    int id;
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
        ""
    },
    .id = 0
};

vfs_file dogescript_example = {
    .name = "dogescript_example",
    .file_extension = "dsc",
    .content = {
        "PRINT HELLO WORLD",
        ""
    },
    .id = 1
};

vfs_file* file_system[MAX_FILES_AMOUNT] = {NULL};
vfs_file file_pool[14];
size_t pool_index = 0;

void file_init_fs() {
    for (int i = 0; i < MAX_FILES_AMOUNT; i++) {
        file_system[i] = NULL;
    }
    file_system[0] = &readme;
    file_system[1] = &dogescript_example;
    file_amount = 2;
}

vfs_file* file_find_by_name(const char* filename) {
    for (int i = 0; i < MAX_FILES_AMOUNT; i++) {
        if (file_system[i] == NULL) continue;
        if (string_strcmp(file_system[i]->name, filename) == 0) {
           return file_system[i];
        }
    }
    return NULL;
}

char (*file_read_file(char* filename))[1024] {
    vfs_file* file = file_find_by_name(filename);
    if (file == NULL) return NULL;
    return file->content;
}

void file_list_files() {
    dogeio_print("Objects in such folder: ");
    dogeio_println("/");
    dogeio_println("");

    dogeio_print("ID");
    dogeio_println("        FILES");
    dogeio_println("-----------");
    for (int i = 0; i < MAX_FILES_AMOUNT; i++) {
        if (file_system[i] != NULL) {
            char numvar[8]; 
            string_itoa(i, numvar);
            dogeio_print(numvar);
            dogeio_print("       ");
            dogeio_print(file_system[i]->name);
            dogeio_print(".");
            dogeio_println(file_system[i]->file_extension);
        }
    }
}

void file_rename_file(char* filename, char* newname) {
    vfs_file* file = file_find_by_name(filename);
    if (file == NULL) return; 
    string_strcpy(file->name, newname);
}

void file_create_file(char* filename, char* file_extension) {
    if (pool_index >= 14) return;

    int slot = -1;
    for (int i = 0; i < MAX_FILES_AMOUNT; i++) {
        if (file_system[i] == NULL) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return;

    vfs_file* new_file = &file_pool[pool_index++];
    string_strcpy(new_file->name, filename);
    string_strcpy(new_file->file_extension, file_extension);
    new_file->id = slot;

    for (int i = 0; i < 64; i++) {
        new_file->content[i][0] = '\0';
    }

    file_system[slot] = new_file;
    file_amount++;
}

void file_delete_file(char* filename) {
    vfs_file* file = file_find_by_name(filename);
    if (file == NULL) return;
    file_system[file->id] = NULL;
    file_amount--;
}

void file_write_file(char* filename, char* string) {
    vfs_file* file = file_find_by_name(filename);
    if (file == NULL) return;

    int num_lines = 0;
    while (num_lines < 64 && file->content[num_lines][0] != '\0') {
        num_lines++;
    }

    if (num_lines >= 63) return;

    char new_file_content[64][1024];
    for (int i = 0; i < 64; i++) {
        if (i < num_lines) {
            string_strcpy(new_file_content[i], file->content[i]);
        } else {
            new_file_content[i][0] = '\0';
        }
    }

    string_strcpy(new_file_content[num_lines], string);

    vfs_file updated_file;
    string_strcpy(updated_file.name, file->name);
    string_strcpy(updated_file.file_extension, file->file_extension);
    updated_file.id = file->id;

    for (int i = 0; i < 64; i++) {
        string_strcpy(updated_file.content[i], new_file_content[i]);
    }

    *file = updated_file;
}

void file_delete_line(vfs_file *file, int line) {
    if (file == NULL) return;

    int num_lines = 0;
    while (num_lines < 64 && file->content[num_lines][0] != '\0') {
        num_lines++;
    }

    if (line < 0 || line >= num_lines) return;

    char new_file_content[64][1024];
    for (int i = 0; i < 64; i++) {
        new_file_content[i][0] = '\0';
    }

    int dest_index = 0;
    for (int src_index = 0; src_index < num_lines; src_index++) {
        if (src_index == line) {
            continue; 
        }
        string_strcpy(new_file_content[dest_index], file->content[src_index]);
        dest_index++;
    }

    vfs_file updated_file;
    string_strcpy(updated_file.name, file->name);
    string_strcpy(updated_file.file_extension, file->file_extension);
    updated_file.id = file->id;

    for (int i = 0; i < 64; i++) {
        string_strcpy(updated_file.content[i], new_file_content[i]);
    }

    *file = updated_file;
}
