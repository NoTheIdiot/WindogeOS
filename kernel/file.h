#ifndef FILE_H
#define FILE_H

// Virtual file system using arrays of strings
// First element is filename, rest are data lines, ended with NULL

extern char* file1[];
extern char* file2[];
extern char* file3[];

void file_read_file(char* file[]);
void file_list_files();
char** file_find_by_name(char* filename);

#endif