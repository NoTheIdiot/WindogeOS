#include <string.h>
#include <system.h>

typedef struct {
    char name[16];
    char extension[4];
    char contents[1024][256]
} file;

file filesystem[16] = {

};

void system_file_readfile(char* filename) {

}

void system_file_list_directory() {

}

void system_file_write_file(char* filename, char* string) {
    
}