#include <string.h>
#include <system.h>

typedef struct {
    char name[16];
    char extension[4];
    char contents[1024][256]
} file;