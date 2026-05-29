#ifndef FILE_H
#define FILE_H

// Virtual file system using arrays of strings
// First element is filename, rest are data lines, ended with NULL

typedef struct {
    char name[32];
    char file_extension[4];
    char content[64][1024];
} vfs_file;

extern vfs_file* file_system[16];

void file_init_filesystem();
void file_read_file(char* filename);
void file_list_files();
vfs_file* file_find_by_name(char* filename);
void file_rename_file(char* filename, const char* new_name);
void file_write_file(char* filename, const char* string);
void file_create_file(char* filename, char* extension);
void file_delete_file(char* filename);

#endif