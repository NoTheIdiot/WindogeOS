#include <stddef.h>
#include <stdint.h>
#include "../helper/dogeio.h"
#include "../helper/string.h"
#include "file.h"


// use extern for now
void doge_script(char* file[]) {
    if (file == NULL || file[0] == NULL) {
        dogeio_print("Error: Invalid file\n");
        return;
    }

    for (int i = 1; file[i] != NULL; i++) {
        char* line = file[i];
        if (string_startswith(line, "PRINT ")) {
            char* message = line + 6;
            
            // f##k this no need for qoutes
            dogeio_println(message);
        }
    }
}
