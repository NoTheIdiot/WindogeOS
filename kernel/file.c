#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "dogeio.h"

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
        ""
    }
};

vfs_file dogescript_example = {
    .name = "dogescript_example",
    .file_extension = "dsc",
    .content = {
        "PRINT HELLO WORLD",
        ""
    }
};

vfs_file* file_system[16] = {NULL};

void file_init_filesystem() {
    file_system[0] = &readme;
    file_system[1] = &dogescript_example;
}

vfs_file* file_find_by_name(const char* filename) {
    for (int i = 0; i < 16; i++) {
        if (file_system[i] == NULL) continue; 
        
        if (string_strcmp(file_system[i]->name, filename) == 0) {
            return file_system[i];
        }
    }
    return NULL; 
}

void file_read_file(char* file_content[]) {
    if (file_content == NULL) {
        dogeio_println("File contents missing. Pls try using much [dir]?");
        return;
    }
    for (int i = 0; i < 64 && file_content[i][0] != '\0'; i++) {
        dogeio_println(file_content[i]);
    }
}

void file_list_files() {
    dogeio_print("ID");
    dogeio_println("    FILES");
    dogeio_println("-----------");
    for (int i = 0; i < 16; i++) {
        if (file_system[i] != NULL) {
            char numvar[8]; 
            string_itoa(i, numvar);
            dogeio_print(numvar);
            dogeio_print("    ");
            dogeio_print(file_system[i]->name);
            dogeio_print(".");
            dogeio_println(file_system[i]->file_extension);
        }
    }
}

void file_rename_file(char* file_content[], const char* new_name) {
    if (file_content == NULL) return;
    vfs_file* file_var = (vfs_file*)((uint8_t*)file_content - offsetof(vfs_file, content));
    string_strncpy(file_var->name, new_name, sizeof(file_var->name) - 1);
    file_var->name[sizeof(file_var->name) - 1] = '\0';
}

void file_write_file(char* file_content[], const char* string) {
    if (file_content == NULL) return;
    
    int i = 0;
    while (i < 64 && file_content[i][0] != '\0') {
        i++;
    }
    
    if (i < 64) {
        string_strncpy(file_content[i], string, 1023);
        file_content[i][1023] = '\0';
        
        if (i + 1 < 64) {
            file_content[i + 1][0] = '\0';
        }
    } else {
        dogeio_println("File full, much error!");
    }
}

void file_delete_line(char* file_content[], int line) {
    if (file_content == NULL) {
        dogeio_print("Error: Invalid file\n");
        return;
    }

    int num_lines = 0;
    while (num_lines < 64 && file_content[num_lines][0] != '\0') {
        num_lines++;
    }

    if (line < 0 || line >= num_lines) {
        dogeio_print("Error: Invalid line number\n");
        return;
    }

    for (int i = line; i < num_lines - 1; i++) {
        string_strcpy(file_content[i], file_content[i + 1]);
    }
    
    file_content[num_lines - 1][0] = '\0';
}

void file_create_file(char* filename) {
    (void)filename;
}

void file_delete_file(char* filename) {
    (void)filename;
}
