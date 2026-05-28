// this took me SO long today
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "dogeio.h"
#include "dogeio_vbe.h"
#include "time.h"
#include "bool.h"
#include "consts.h"
#include "file.h"
#include "info.h"
#include "multiboot.h"

extern char* such_windoge_version;
extern char* such_windoge_version_short;

char* shell_get_arg(char* buffer, int command_len) {
    char* arg = buffer + command_len;
    while (*arg == ' ' && *arg != '\0') {
        arg++;
    }
    return (*arg == '\0') ? NULL : arg;
}

void doge_shell() {
    char command_buffer[128];

    while (true) {
        int handled = 0;
        dogeio_print("windoge~# ");
        dogeio_input(command_buffer, 128, LIGHT_BROWN);

        int len = string_strlen(command_buffer);
        while (len > 0 && (command_buffer[len - 1] == '\n' || command_buffer[len - 1] == '\r')) {
            command_buffer[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            continue;
        }

        if (string_startswith(command_buffer, "print")) {
            char* arg = shell_get_arg(command_buffer, 5);
            dogeio_println(arg ? arg : "");
            handled = 1;
        } 
        else if (string_startswith(command_buffer, "bark")) {
            char* arg = shell_get_arg(command_buffer, 4);
            dogeio_println(arg ? arg : "Woof!");
            handled = 1;
        }
        else if (string_strcmp(command_buffer, "clear") == 0) {
            dogeio_clear_screen();
            handled = 1;
        }
        else if (string_strcmp(command_buffer, "halt") == 0) {
            dogeio_clear_screen();
            dogeio_println("Halting such CPU, very goodbye.");
            while (true) { __asm__ volatile ("hlt"); }
        }
        else if (string_strcmp(command_buffer, "time") == 0 || string_strcmp(command_buffer, "date") == 0) {
            time_update_time();
            time_show();
            handled = 1;
        }

        // not for now
        else if (string_startswith(command_buffer, "info")) {
            handled = 1;
        }

        else if (string_strcmp(command_buffer, "dir") == 0) {
            file_list_files();
            handled = 1;
        } 
        else if (string_startswith(command_buffer, "read")) {
            char* file = shell_get_arg(command_buffer, 4);
            vfs_file* target = file_find_by_name(file);
            
            if (target == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                for (int i = 0; i < 64 && target->content[i][0] != '\0'; i++) {
                    dogeio_println(target->content[i]);
                }
            }
            handled = 1;
        } 
        else if (string_startswith(command_buffer, "rename")) {
            char* file = shell_get_arg(command_buffer, 6);
            vfs_file* target = file_find_by_name(file);
            
            if (target == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                char new_name[16];
                dogeio_println("Enter the new name pls:");
                dogeio_input(new_name, 16, LIGHT_BROWN);
                
                int nn_len = string_strlen(new_name);
                while (nn_len > 0 && (new_name[nn_len - 1] == '\n' || new_name[nn_len - 1] == '\r')) {
                    new_name[nn_len - 1] = '\0';
                    nn_len--;
                }
                string_strncpy(target->name, new_name, sizeof(target->name) - 1);
                target->name[sizeof(target->name) - 1] = '\0';
            }
            handled = 1;
        } 
        else if (string_startswith(command_buffer, "write")) {
            char* file = shell_get_arg(command_buffer, 5);
            vfs_file* target = file_find_by_name(file);
            
            if (target == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                char content[128];
                dogeio_println("Enter the new content to the next line:");
                dogeio_input(content, 128, LIGHT_BROWN);
                
                int c_len = string_strlen(content);
                while (c_len > 0 && (content[c_len - 1] == '\n' || content[c_len - 1] == '\r')) {
                    content[c_len - 1] = '\0';
                    c_len--;
                }
                
                int line = 0;
                while (line < 64 && target->content[line][0] != '\0') {
                    line++;
                }
                if (line < 64) {
                    string_strncpy(target->content[line], content, 1023);
                    target->content[line][1023] = '\0';
                } else {
                    dogeio_println("File full!");
                }
            }
            handled = 1;
        } else if (string_startswith(command_buffer, "multiwrite")) {
            char* file = shell_get_arg(command_buffer, 10);
            vfs_file* target = file_find_by_name(file);

            if (target == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                char line_buffer[128];
                char istring[8];
                int line_counter = 1;
                
                dogeio_println("Enter such content (empty line to stop):");
                while (true) {
                    string_itoa(line_counter, istring);
                    dogeio_print(istring);
                    dogeio_print(" ");
                
                    dogeio_input(line_buffer, 128, LIGHT_BROWN);
                    int len = string_strlen(line_buffer);
                    while (len > 0 && (line_buffer[len - 1] == '\n' || line_buffer[len - 1] == '\r')) {
                        line_buffer[len - 1] = '\0';
                        len--;
                    }

                    if (len == 0) {
                        break;
                    }
                    
                    int line = 0;
                    while (line < 64 && target->content[line][0] != '\0') {
                        line++;
                    }
                    if (line < 64) {
                        string_strncpy(target->content[line], line_buffer, 1023);
                        target->content[line][1023] = '\0';
                    }
                    line_counter++;
                }
            }
            handled = 1;
        } else if (string_startswith(command_buffer, "deleteline")) {
            char* filename = shell_get_arg(command_buffer, 10);
            vfs_file* target = file_find_by_name(filename);

            if (target == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                char line_num_buf[16];
                dogeio_println("Enter line number to delete:");
                dogeio_input(line_num_buf, 16, LIGHT_BROWN);

                int l_len = string_strlen(line_num_buf);
                while (l_len > 0 && (line_num_buf[l_len - 1] == '\n' || line_num_buf[l_len - 1] == '\r')) {
                    line_num_buf[l_len - 1] = '\0';
                    l_len--;
                }

                int line_to_delete = string_atoi(line_num_buf);
                line_to_delete--;
                int total_lines = 0;
                while (total_lines < 64 && target->content[total_lines][0] != '\0') {
                    total_lines++;
                }

                if (line_to_delete >= 0 && line_to_delete < total_lines) {
                    for (int i = line_to_delete; i < total_lines - 1; i++) {
                        string_strcpy(target->content[i], target->content[i + 1]);
                    }
                    target->content[total_lines - 1][0] = '\0';
                } else {
                    dogeio_println("The line number you such tried to delete has the existance level of my dad, try again :(");
                }
            }
            handled = 1;
        } 
        else if (string_startswith(command_buffer, "dogescript")) {
            char* file = shell_get_arg(command_buffer, 10);
            vfs_file* target = file_find_by_name(file);
            
            if (target == NULL) {
                dogeio_println("file not found, try refering to dir");
            } else {
                int loop_start_line = -1;
                int loop_iterations = 0;

                for (int k = 0; k < 64 && target->content[k][0] != '\0'; k++) {
                    char* line = target->content[k];
                    if (string_startswith(line, "PRINT")) {
                        dogeio_println(line + 6);
                    } else if (string_startswith(line, "BARK")) {
                        dogeio_println(line + 5);
                    } else if (string_startswith(line, "SHELL")) {
                        // make a shell command?
                    } else if (string_strcmp(line, "LOOP") == 0) {
                        if (loop_start_line == -1) {
                            loop_start_line = k;
                            loop_iterations = 0;
                        }
                    } else if (string_strcmp(line, "END") == 0) {
                        if (loop_start_line != -1) {
                            loop_iterations++;
                            if (loop_iterations < 5) {
                                k = loop_start_line;
                            } else {
                                loop_start_line = -1;
                            }
                        }
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
