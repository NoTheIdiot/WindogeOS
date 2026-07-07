#include <string.h>
#include <dogeio.h>
#include <system.h>
#include <bool.h>

/* built in files */
file_t readme = {
    .name = "readme",
    .extension = "txt",
    .content = {
        "Welcome to WindogeOS"
    }
};

file_t random = {
    .name = "random",
    .extension = "txt",
    .content = {
        "Trhopi drv;hk eg; ufhs"
    }
};
/* end */

file_t filesystem[16] = {
};

file_t* system_file_search(char* filename) {
    for (int i = 0; i < 16; i++) {
        if (string_strcmp(filename, filesystem[i].name) == 0) {
            return &filesystem[i];
        }
    }
    return NULL;
}

void system_file_init() {
    for (int i = 0; i < 16; i++) {
        filesystem[i].name[0] = '\0';
        filesystem[i].extension[0] = '\0';
        for (int j = 0; j < 1024; j++) {
            filesystem[i].content[j][0] = '\0';
        }
    }
    filesystem[0] = readme;
    filesystem[1] = random;
}

void system_file_readfile(char* filename) {
    file_t* file = system_file_search(filename);
    if (file == NULL) {
        dogeio_text_println("Error: File Not Found.");
        return;
    }

    for (int i = 0; i < 1024; i++) {
        if (file->content[i][0] == '\0') {
            break; 
        }
        dogeio_text_println(file->content[i]);
    } 
}

void system_file_list_directory() {
    for (int i = 0; i < 16; i++) {
        if (filesystem[i].name[0] != '\0') {
            dogeio_text_print(filesystem[i].name);
            dogeio_text_print(".");
            dogeio_text_println(filesystem[i].extension);
        }
    }
}

void system_file_write_file(char* filename, char* string) {
    file_t* file = system_file_search(filename);
    if (file == NULL) {
        dogeio_text_println("Error: File Not Found.");
        return;
    }
    
    for (int i = 0; i < 1024; i++) {
        if (file->content[i][0] == '\0') {
            string_strcpy(file->content[i], string);
            return;
        }
    }
    
    dogeio_text_println("Error: File is full.");
}
