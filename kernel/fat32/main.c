#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "fat32.h"

char* text = "In the world of technology, progress is an unstoppable force. Every year, new innovations emerge, reshaping the way we live and work. From computers to mobile devices, technology has revolutionized communication, education, and entertainment. It connects people across the globe in ways that were once unimaginable. However, with all these advancements, challenges also arise. Issues like data privacy, cybersecurity, and ethical implications of AI are becoming more prominent. As technology continues to evolve, it's important to balance innovation with responsibility. The future of tech is both exciting and uncertain, but it holds the potential to solve some of the world's most pressing problems. By working together and embracing both creativity and caution, we can ensure that the next generation of technology is not only groundbreaking but also beneficial to society as a whole.";

int main(int argc, char* argv[])
{
    disk_init("fat32.img");
    fat32_init();

    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s <path>\n", argv[0]);
        return -1;
    }

    // fat32_make(argv[1], true);

    // dirEntry_t file;
    // file_t* root = fat32_open("/");

    // printf("Root directory:\n");
    // while (fat32_readdir(root, &file) != 0)
    // {
    //     // printf("%-*s (%s)\n", 40, file.name, file.type == DIR_TYPE ? "directory" : "file");
    //     char* path = malloc(strlen(file.name) + 2);
    //     strcpy(path, "/");
    //     strcat(path, file.name);

    //     printf("path: %s\n", path);
    //     // fat32_delete(path);
    // }

    fat32_make("/test directory", true);
    fat32_make("/test directory/my super file.txt", false);

    file_t* super_file = fat32_open("/test directory/my super file.txt");

    fat32_write(super_file, text, 887);

    char* buf[1000];

    super_file->readPos = 0;    // seek
    fat32_read(super_file, buf, 887);

    printf("content: \n%s\n", buf);

    return 0;
}