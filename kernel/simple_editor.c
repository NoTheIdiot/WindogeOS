/* I f##king made this in 19 minutes */
#include <string.h>
#include <bool.h>
#include <system.h>
#include <dogeio.h>

char* help[] = {
    "i                  | insert text straight to file",
    "o                  | outputs the contents of a file",
    "d                  | deletes the last line of a file",
    "c                  | clears screen",
    "e                  | exits the editor"
};

int editor_shell_ex(char* command, char* filename) {
    if (!command) {
        return 0;
    }

    if (str_strcmp(command, "i") == 0) {
        char input[512];
        char i_str[8];
        int total_lines = 0;

        dogeio_text_println("Type . on the next line only to exit insert.");
        
        while (true) {
            str_itoa(total_lines, i_str);
            dogeio_text_print(i_str);
            dogeio_text_input("> ", input, 512);
            
            if (str_strcmp(input, ".") == 0) {
                break;
            }
            
            fs_write(filename, input); 
            total_lines++;
        }
        dogeio_text_println("Insert ended.");

    } else if (str_strcmp(command, "o") == 0) {
        static char output_buffer[8192];

        int bytes_read = fs_read(filename, output_buffer, sizeof(output_buffer));
        if (bytes_read < 0) {
            dogeio_text_println("Error: unable to read file.");
        } else {
            char *line = output_buffer;
            int processed = 0;
            while (processed < bytes_read) {
                dogeio_text_println(line);
                size_t line_len = str_strlen(line);
                processed += (int)line_len + 1;
                line += line_len + 1;
            }
        }
    } else if (str_strcmp(command, "d") == 0) {
        fs_delete_last_line(filename);
    } else if (str_strcmp(command, "c") == 0) {
        dogeio_text_clear();
        for (int i = 0; i < 5; i++) {
            dogeio_text_println(help[i]);
        }
    } else {
        dogeio_text_println("Command Doesn't Exist.");
    }
    return 1;
}

void editor(char* filename) {
    char input[16]; 

    for (int i = 0; i < 5; i++) {
        dogeio_text_println(help[i]);
    }
    dogeio_text_println("Welcome To The WindogeOS Very Basic Code Editor!");
    dogeio_text_println("This is seriously just better ed, use e to exit.");
    
    while (true) {
        dogeio_text_input("> ", input, 16); 
        if (str_strcmp(input, "e") == 0) {
            dogeio_text_println("Exiting Very Basic Text Editor.");
            return;
        }
        editor_shell_ex(input, filename);
    }
}
