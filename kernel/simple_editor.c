#include <string.h>
#include <bool.h>
#include <system.h>
#include <dogeio.h>

static const char* dogeedit_help[] = {
    "i                  | insert text straight to file",
    "o                  | outputs the contents of a file",
    "d                  | deletes the last line of a file",
    "c                  | clears screen & shows help",
    "s                  | prints file details / status",
    "e                  | saves and exits dogeedit"
};

int dogeedit_shell_ex(char* command, char* filename) {
    if (!command || command[0] == '\0') {
        return 0;
    }

    if (str_strcmp(command, "i") == 0) {
        char input[512];
        char i_str[16];
        int total_lines = 0;

        if (!fs_exists(filename)) {
            fs_create(filename);
        }

        dogeio_text_println("Type . on a new line to exit insert mode.");
        
        while (true) {
            str_itoa(total_lines, i_str);
            dogeio_text_print(i_str);
            dogeio_text_input("> ", input, 508);
            
            if (str_strcmp(input, ".") == 0) {
                break;
            }
            
            // Append newline so text is stored properly line-by-line
            size_t len = str_strlen(input);
            input[len] = '\n';
            input[len + 1] = '\0';

            fs_write(filename, input); 
            total_lines++;
        }
        dogeio_text_println("Insert ended.");

    } else if (str_strcmp(command, "o") == 0) {
        static char output_buffer[8192];

        int bytes_read = fs_read(filename, output_buffer, sizeof(output_buffer) - 1);
        if (bytes_read < 0) {
            dogeio_text_println("Error: unable to read file.");
        } else if (bytes_read == 0) {
            dogeio_text_println("(File is empty)");
        } else {
            output_buffer[bytes_read] = '\0';
            dogeio_text_print(output_buffer);
            dogeio_text_println("");
        }

    } else if (str_strcmp(command, "d") == 0) {
        if (fs_exists(filename)) {
            fs_delete_last_line(filename);
            dogeio_text_println("Last line removed.");
        } else {
            dogeio_text_println("Error: File does not exist.");
        }

    } else if (str_strcmp(command, "c") == 0) {
        dogeio_text_clear();
        for (int i = 0; i < 6; i++) {
            dogeio_text_println(dogeedit_help[i]);
        }

    } else if (str_strcmp(command, "s") == 0) {
        if (!fs_exists(filename)) {
            dogeio_text_print("File: ");
            dogeio_text_println(filename);
            dogeio_text_println("Size: 0 bytes");
            dogeio_text_println("Lines: 0 (New File)");
        } else {
            static char status_buffer[8192];
            int bytes = fs_read(filename, status_buffer, sizeof(status_buffer) - 1);
            if (bytes < 0) {
                dogeio_text_println("File status unavailable.");
            } else {
                status_buffer[bytes] = '\0';
                int line_count = 0;
                for (int i = 0; i < bytes; i++) {
                    if (status_buffer[i] == '\n') {
                        line_count++;
                    }
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
        dogeio_text_println("Command Doesn't Exist. Type 'c' for help.");
    }
    return 1;
}

void dogeedit(char* filename) {
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
        dogeio_text_println(dogeedit_help[i]);
    }
    dogeio_text_println("Dogedit v1.3");
    dogeio_text_println("Type 'e' to save & exit.");
    
    while (true) {
        dogeio_text_input("> ", input, 16); 
        if (str_strcmp(input, "e") == 0) {
            if (fs_exists(filename)) {
                fs_delete(filename);
            }
            
            if (fs_exists(swap_filename)) {
                fs_copy(swap_filename, filename);
                fs_delete(swap_filename);
            }
            
            dogeio_text_println("Exiting dogeedit. File saved!");
            return;
        }
        dogeedit_shell_ex(input, swap_filename);
    }
}