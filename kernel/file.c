// please fucking help me
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

void file_read_file(const char* file) {
    vfs_file* file_var = file_find_by_name(file);
    if (file_var == NULL) {
        dogeio_println("File isn't found. Pls try using much [dir]?");
    } else {
        for (int i = 0; i < 64 && file_var->content[i][0] != '\0'; i++) {
            dogeio_println(file_var->content[i]);
        }
    }
}

void file_list_files() {
    dogeio_print("ID");
    dogeio_println("    FILES");
    dogeio_println("-----------");
    for (int i = 0; i < 16; i++) {
        if (file_system[i] != NULL) {
            // i keep forgeting that i don't have this function
            char* numvar = "";
            string_itoa(i, numvar);
            dogeio_print(numvar);
            dogeio_print("    ");
            dogeio_println(file_system[i]->name);
        }
    }
}

void file_rename_file(const char* filename, const char* new_name) {
    vfs_file* file_var = file_find_by_name(filename);
    if (file_var == NULL) { 
        dogeio_println("File not found, not wow. Try using many [dir]?"); 
        return; 
    }
    string_strncpy(file_var->name, new_name, sizeof(file_var->name) - 1);
    file_var->name[sizeof(file_var->name) - 1] = '\0';
}

void file_write_file(const char* filename, const char* string) {
    vfs_file* file_var = file_find_by_name(filename);
    if (file_var == NULL) { 
        dogeio_println("File not found, not wow. Try using many [dir]?"); 
        return; 
    }
    
    int i = 0;
    while (i < 64 && file_var->content[i][0] != '\0') {
        i++;
    }
    
    if (i < 64) {
        string_strncpy(file_var->content[i], string, sizeof(file_var->content[i]) - 1);
        file_var->content[i][sizeof(file_var->content[i]) - 1] = '\0';
        
        if (i + 1 < 64) {
            file_var->content[i + 1][0] = '\0';
        }
    } else {
        dogeio_println("File full, much error!");
    }
}

void file_delete_line(vfs_file* file, int line) {
    if (file == NULL) {
        dogeio_print("Error: Invalid file\n");
        return;
    }

    int num_lines = 0;
    while (num_lines < 64 && file->content[num_lines][0] != '\0') {
        num_lines++;
    }

    if (line < 0 || line >= num_lines) {
        dogeio_print("Error: Invalid line number\n");
        return;
    }

    for (int i = line; i < num_lines - 1; i++) {
        string_strcpy(file->content[i], file->content[i + 1]);
    }
    
    file->content[num_lines - 1][0] = '\0';
}

void file_create_file(char* filename) {
    
}