#include <stddef.h>
#include <stdint.h>
#include "dogeio.h"
#include "string.h"
#include "file.h"
#include "dogeshell.h"

// use extern for now
void doge_script(vfs_file* file) {
    for (int i = 1; i < 64; i++) {
        char* line = file->content[i];

		if (line[0] == '\0') {
			break;
		}
        
        if (string_startswith(line, "PRINT ")) {
            char* message = line + 6;
            
            // f##k this no need for qoutes
            dogeio_println(message);
            
        } else if (string_startswith(line, "BARK ")) {
        	char* message = line + 5;
        	dogeio_println(message);
        	
        } else if (string_startswith(line, "SHELL ")) {
        	char* shell_command = line + 6;
        	dogeshell_execute(shell_command);
        	
        } else {
        	dogeio_print(line);
        	dogeio_println(" <~ [1] command not found, not wow.");
        	dogeio_println("Execution has stopped.");
        }
    }
}
