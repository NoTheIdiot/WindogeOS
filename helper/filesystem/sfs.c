/*
random notes:
 - to anyone who wants to modify and call it your own, pls do delete this to avoid
   your drama

This is just the development of an actualy file system, do not delete fs.c.
This is a part of dogeio header file.
*/

#include <dogeio.h>
#include <stdint.h>
#include <stddef.h>

// system memory config.
// simply so the FS doesn't break the kernel itself.
static struct fs_layout *fs = (struct fs_layout*)0x5000000;


/*
format the disk by just spamming zeros, also known as 0x00.
also deletes the memory space.
*/
void fs_format() {
    // put magic
    fs->magic = 0x444f4745;
    fs->total_blocks = TOTAL_DATA_BLOCKS;

    for (int i = 0; i < MAX_FILES; i++) {
        fs->file_table[i].used = 0;
        fs->file_table[i].size = 0;
        
        fs->file_table[i].start_block = (uint32_t)i * BLOCKS_PER_FILE;
        
        for (int j = 0; j < MAX_FILENAME; j++) {
            fs->file_table[i].filename[j] = 0;
        }
    }
}

// creates file, obvious by function name.]
// returns file descriptor id int on sucess, and -1 is shit goes out bad.
int fs_create(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->file_table[i].used == 0) {
            fs->file_table[i].used = 1;
            fs->file_table[i].size = 0;
            
            int j = 0;
            while (name[j] != '\0' && j < (MAX_FILENAME - 1)) {
                fs->file_table[i].filename[j] = name[j];
                j++;
            }
            fs->file_table[i].filename[j] = '\0';
            
            return i; 
        }
    }
    // too full, no indexing left.
    return -1;
}