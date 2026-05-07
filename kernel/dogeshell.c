// include files
#include <stddef.h>
#include <stdint.h>
#include "../helper/string.h"
#include "../helper/dogeio.h"
#include "../helper/dogeio_graphics.h"
#include "../helper/time.h"
#include "../helper/bool.h"
#include "consts.h"
#include "file.h"
#include "info.h"
#include "multiboot.h"


// extern variables
extern char* such_windoge_version;
extern char* such_windoge_version_short;

// the shell, use extern for now
void doge_shell() {
    
    // variables needed
    char command_buffer[128];
    int handled = 0;

    // the shell itself
    while (true) {
        dogeio_print("windoge~# ");
        dogeio_input(command_buffer, 128, LIGHT_BROWN);

        // do nothing is the buffer is empty
        if (string_strlen(command_buffer) == 0) {
            continue;
        }

        // actual commands
        if (string_startswith(command_buffer, "print")) {
            dogeio_println(command_buffer + 6);
            handled = 1;

        } else if (string_startswith(command_buffer, "bark")) {
            dogeio_println(command_buffer + 5);
            handled = 1;
            
        }

        if (string_strcmp(command_buffer, "halt") == 0) {
            dogeio_clear_screen();
            dogeio_println("Halting such CPU, very goodbye.");
            while (true) { __asm__ volatile ("hlt"); }

        }

        if (string_strcmp(command_buffer, "clear") == 0) {
            dogeio_clear_screen();
            handled = 1;

        }

        // time commands
        if (string_strcmp(command_buffer, "time") == 0 || string_strcmp(command_buffer, "date") == 0) {
            time_update_time();
            time_show();
            handled = 1;

        }


        if (string_startswith(command_buffer, "info")) {
            char* info_arg = command_buffer + 5;
            int info_handled = 0;
            if (string_strcmp(info_arg, "os")) {
                dogeio_println("WindogeOS Info: ");
                dogeio_print("Version: ");
                dogeio_println(such_windoge_version);
                info_handled = 1;

            } else if (info_arg == NULL || string_strlen(info_arg) == 0) {
                info_handled = 0;

            } else {
                dogeio_println("unkown argument, pls use multiboot or os");
            }

            if (info_handled != 1) {
                dogeio_println("info: info <argument>");
            } else {
                handled = 1;
            }

        }

        // virtual file system commands
        if (string_strcmp(command_buffer, "dir") == 0) {
            file_list_files();
            handled = 1;

        } else if (string_startswith(command_buffer, "read")) {
            char* file = command_buffer + 5;
            char** file_input = file_find_by_name(file);
            if (file_input == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                file_read_file(file_input);
            }
            handled = 1;

        } else if (string_startswith(command_buffer, "rename")) {
            char* old_name = command_buffer + 7;
            char new_name[16];
            char** file_input = file_find_by_name(old_name);
            if (old_name == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                dogeio_println("Enter the new name pls:");
                dogeio_input(new_name, 16, LIGHT_BROWN);
                file_rename_file(file_input, new_name);
            }
            handled = 1;

        } else if (string_startswith(command_buffer, "write")) {
            char* file = command_buffer + 6;
            char content[128];
            char** file_input = file_find_by_name(file);
            if (file_input == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                dogeio_println("Enter the new content to the next line:");
                dogeio_input(content, 128, LIGHT_BROWN);
                file_write_file(file_input, content);
            }
            handled = 1;

            // why doesn't this work???
        } else if (string_startswith(command_buffer, "multiwrite")) {
            char* file = command_buffer + 11;
            char total_content[512] = {0}; 
            char line_buffer[128];
            int current_pos = 0;
            int i = 1;
            char istring[8];

            char** file_input = file_find_by_name(file);
            if (file_input == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                dogeio_println("Enter such content (empty line to stop):");
                while (true) {
                    string_itoa(i, istring);
                    dogeio_print(istring);
                    dogeio_print(" ");
                
                    dogeio_input(line_buffer, 128, LIGHT_BROWN);
                    int len = string_strlen(line_buffer);

                    if (len == 0) break;

                    for (int j = 0; j < len; j++) {
                        if (current_pos < 510) {
                            total_content[current_pos++] = line_buffer[j];
                        }
                    }
            
                        if (current_pos < 510) {
                            total_content[current_pos++] = '\n';
                        }
            
                        total_content[current_pos] = '\0';
                        i++;
                    }
                    file_write_file(file_input, total_content);
            }
            handled = 1;
        } else if (string_startswith(command_buffer, "deleteline")) {
            char* file = command_buffer + 11;
            char line_str[8];
            int line_num;
            char** file_input = file_find_by_name(file);
            if (file_input == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                dogeio_println("Enter the line number to delete:");
                dogeio_input(line_str, 8, LIGHT_BROWN);
                line_num = string_atoi(line_str);
                file_delete_line(file_input, line_num);
            }
            handled = 1;

        } else if (string_startswith(command_buffer, "dogescript")) {
            char* file = command_buffer + 11;
            char** file_input = file_find_by_name(file);
            if (file_input == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                for (int i = 1; file_input[i] != NULL; i++) {
                    char* line = file_input[i];
                    if (string_startswith(line, "PRINT")) {
                        dogeio_println(line + 6);
                    } else if (string_startswith(line, "BARK")) {
                        dogeio_println(line + 5);
                    } else {
                        dogeio_print("Unknown command in script: ");
                        dogeio_println(line);
                    }
                }
            }
            handled = 1;
        }


        if (!handled) {
            dogeio_print(command_buffer);
            dogeio_println(": command not such found :(");
        }
    }
}