// include files
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
extern char* boot_time;

extern void system_sysinfo(void);

char* shell_get_arg(char* buffer, int command_len) {
    char* arg = buffer + command_len;
    while (*arg == ' ' && *arg != '\0') {
        arg++;
    }
    return (*arg == '\0') ? NULL : arg;
}

void dogeshell_execute(char* command_buffer) {
    int handled = 0;
    char create_buffer[8];
    
    int len = string_strlen(command_buffer);
    while (len > 0 && (command_buffer[len - 1] == '\n' || command_buffer[len - 1] == '\r')) {
        command_buffer[len - 1] = '\0';
        len--;
    }

    if (len == 0) {
        return;
    }

    if (string_startswith(command_buffer, "print")) {
        char* arg = shell_get_arg(command_buffer, 5);
        dogeio_println(arg ? arg : "");
        handled = 1;
    } 
    else if (string_startswith(command_buffer, "bark")) {
        char* arg = shell_get_arg(command_buffer, 4);
        dogeio_println(arg ? arg : "");
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
        dogeio_print("\n");
        handled = 1;
    } 
    else if (string_strcmp(command_buffer, "sysinfo") == 0) {
        system_sysinfo(); // what
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
            file_read_file(file);
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
    } 
    else if (string_startswith(command_buffer, "multiwrite")) {
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
    } 
    else if (string_startswith(command_buffer, "deleteline")) {
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
                dogeio_println("The line number you such tried to delete has the existence level of my dad, try again :(");
            }
        }
        handled = 1;
    } else if (string_startswith(command_buffer, "dogescript")) {
        char* file = shell_get_arg(command_buffer, 10);
        vfs_file* target = file_find_by_name(file);
        
        if (target == NULL) {
            dogeio_println("file not found, try refering to dir");
        } else {
            int s_line = 0;
            while (s_line < 64 && target->content[s_line][0] != '\0') {
                dogeshell_execute(target->content[s_line]);
                s_line++;
            }
        }
        handled = 1;
    } else if (string_startswith(command_buffer, "deletefile")) {
        char* file = shell_get_arg(command_buffer, 10);
        vfs_file* target = file_find_by_name(file);

        if (target == NULL) {
            dogeio_println("File not found, try refering to [dir]");
        } else {
            file_delete_file(file);
        }
        handled = 1;
    } 
    else if (string_startswith(command_buffer, "createfile")) {
        char* file = shell_get_arg(command_buffer, 10);
        if (file == NULL || string_strlen(file) == 0) {
            dogeio_println("usage: createfile <filename>");
        } else {
            vfs_file* target = file_find_by_name(file);
            if (target != NULL) {
                dogeio_println("file already exists, pls try another name?");
            } else {
            	dogeio_println("File extension?");
            	dogeio_input(create_buffer, 8, 0x0E);
                file_create_file(file, create_buffer); 
            }
        }
        handled = 1;
    }

    if (!handled) {
        dogeio_println("Such unknown command, much confuse.");
    }
}

void doge_shell() {
    char command_buffer[128];

    while (true) {
        dogeio_print("windoge~# ");
        dogeio_input(command_buffer, 128, LIGHT_BROWN);
        dogeshell_execute(command_buffer);
    }
}
