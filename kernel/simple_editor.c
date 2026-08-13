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
    "s                  | prints file details / status",
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

        if (!fs_exists(filename)) {
            fs_create(filename);
        }

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
                if (*line == '\0') {
                    processed++;
                    line++;
                    continue;
                }
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
        for (int i = 0; i < 6; i++) {
            dogeio_text_println(help[i]);
        }
    } else if (str_strcmp(command, "s") == 0) {
        if (!fs_exists(filename)) {
            dogeio_text_print("File: ");
            dogeio_text_println(filename);
            dogeio_text_println("Size: 0 bytes");
            dogeio_text_println("Lines: 0 (New File)");
        } else {
            static char status_buffer[8192];
            int bytes = fs_read(filename, status_buffer, sizeof(status_buffer));
            if (bytes < 0) {
                dogeio_text_println("File status unavailable.");
            } else {
                int line_count = 0;
                int processed = 0;
                char *line = status_buffer;
                while (processed < bytes) {
                    if (*line == '\0') {
                        processed++;
                        line++;
                        continue;
                    }
                    line_count++;
                    size_t line_len = str_strlen(line);
                    processed += (int)line_len + 1;
                    line += line_len + 1;
                }
                char num_str[16];
                dogeio_text_print("File: ");
                dogeio_text_println(filename);
                str_itoa(bytes, num_str);
                dogeio_text_print("Size: ");
                dogeio_text_print(num_str);
                dogeio_text_println(" bytes");
                str_itoa(line_count, num_str);
                dogeio_text_print("Lines: ");
                dogeio_text_println(num_str);
            }
        }
    } else {
        dogeio_text_println("Command Doesn't Exist.");
    }
    return 1;
}

void editor(char* filename) {
    char input[16]; 
    char swap_filename[256];

    str_strcpy(swap_filename, filename);
    str_strcat(swap_filename, ".swp");

    if (fs_exists(swap_filename)) {
        dogeio_text_println("Swap file detected from a previous crash.");
        dogeio_text_input("Recover session? (y/n): ", input, 16);
        if (str_strcmp(input, "y") != 0) {
            fs_delete(swap_filename);
        }
    }

    if (!fs_exists(swap_filename)) {
        if (fs_exists(filename)) {
            fs_copy(filename, swap_filename); 
        } else {
            fs_create(swap_filename);
        }
    }

    for (int i = 0; i < 6; i++) {
        dogeio_text_println(help[i]);
    }
    dogeio_text_println("Welcome To The WindogeOS Very Basic Code Editor!");
    dogeio_text_println("This is seriously just better ed, use e to exit.");
    
    while (true) {
        dogeio_text_input("> ", input, 16); 
        if (str_strcmp(input, "e") == 0) {
            if (!fs_exists(filename)) {
                fs_create(filename);
            } else {
                fs_delete(filename);
                fs_create(filename);
            }

            if (fs_exists(swap_filename)) {
                fs_copy(swap_filename, filename);
                fs_delete(swap_filename);
            }
            
            dogeio_text_println("Exiting Very Basic Text Editor.");
            return;
        }
        editor_shell_ex(input, swap_filename);
    }
}