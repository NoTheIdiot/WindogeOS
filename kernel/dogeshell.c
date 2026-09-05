#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <system.h>
#include <core.h>
#include <basicutil.h>
#include <image.h>
#include <bool.h>

uint32_t saved_color = 0xFFCCCCCC;
char user[64];
char old2[64];
static char buffer[8192];
char* help[] = {
    "Basic Functions",
    "=======================================================",
    "print  [text]           | prints text",
    "clear                   | clears terminal",
    "ver                     | shows shell version",
    "history                 | shows shell history",
    "clear-history           | clears shell history",
    "time                    | shows time",
    "shutdown                | shuts down system",
    "reboot                  | restarts/reboot the system",
    "=======================================================",
    "",
    "File System",
    "=======================================================",
    "dir    [location]       | lists folder",
    "read   [file]           | outputs file contents",
    "write  [file]           | writes contents into files",
    "del    [file]           | deletes a file",
    "create [file]           | creates a new file",
    "rename [file] [name]    | renames a file",
    "whereami                | shows current location",
    "=======================================================",
    "",
    "System Information",
    "=======================================================",
    "whoami                  | shows current user",
    "cpuinfo                 | show CPU name",
    "raminfo                 | show RAM amount in bytes",
    "time                    | shows time",
    "date                    | shows the date and time",
    "fetch                   | just like fastfetch",
    "=======================================================",
    "",
    "System Utilities",
    "=======================================================",
    "edit                    | edits a file",
    "genimg                  | generates a solid color image",
    "viewimg                 | views solid color image",
    "dogescript              | runs dogescript scripts",
    "=======================================================",
};

int system_dogeshell_ex(char* command) {
    int handled = 1;

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    // basic functions
    if (str_strcmp(command, "print") == 0 || str_startswith(command, "print ")) {
        if (str_startswith(command, "print ")) {
            dogeio_text_println(command + 6);
        } else {
            dogeio_text_println(""); 
        }
        handled = 0;
    }

    else if (str_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 0;
    }

    else if (str_strcmp(command, "ver") == 0) {
        dogeio_text_println(dogeshell_version);
        handled = 0;
    }

    else if (str_strcmp(command, "history") == 0) {
        fs_chdir(user);
        int bytes_read = fs_read(".history", buffer, 8192);
        if (bytes_read <= 0) {
            dogeio_text_println("No history available.");
        } else {
            char *line = buffer;
            size_t processed = 0;
            while ((int)processed < bytes_read) {
                dogeio_text_println(line);
                size_t line_len = str_strlen(line);
                processed += line_len + 1;
                line += line_len + 1;
            }
        }
        handled = 0;
    }

    else if (str_strcmp(command, "help") == 0) {
        size_t count = sizeof(help) / sizeof(help[0]);
        for (size_t i = 0; i < count; i++) {
            dogeio_text_println(help[i]);
        }
        handled = 0;
    }

    // shutting down
    else if (str_strcmp(command, "shutdown") == 0 || str_strcmp(command, "poweroff") == 0) {
        core_shutdown();
        handled = 0;
    }

    // rebooting
    else if (str_strcmp(command, "reboot") == 0 || str_strcmp(command, "restart") == 0) {
        core_reboot();
        handled = 0;
    }


    /*  file system functions, and this is a human not a fucking clanker  */
    else if (str_startswith(command, "dir")) {
        char flags[16];

        if (str_strcmp(flags, "--hidden")) {
            fs_list_dir(1);
        } else {
            fs_list_dir(0);
        }
        handled = 0;
    }

    // read a file, more of a cat than a read
    else if (str_startswith(command, "read")) {
        char* file = command + 5;
        if (!fs_exists(file)) {
            dogeio_text_println("Much Sad: file doesn't exist.");
            handled = -1;
        } else {
            int bytes_read = fs_read(file, buffer, sizeof(buffer));
            if (bytes_read < 0) {
                dogeio_text_println("Not Wow: unable to read file.");
            } else {
                char *line = buffer;
                size_t processed = 0;
                while ((int)processed < bytes_read) {
                    dogeio_text_println(line);
                    size_t line_len = str_strlen(line);
                    processed += line_len + 1;
                    line += line_len + 1;
                }
            }
        }
    }

    // write a file (also known as sed)
    else if (str_startswith(command, "write")) {
    	char* file = command + 6;

    	if (!fs_exists(file)) {
    		dogeio_text_println("Much Error: file doesn't exist");
    		handled = -1;
    	} else {
    		dogeio_text_input("> ", buffer, 8192);
            fs_write(file, buffer);
            handled = 0;
    	}
    }

    // rename a file
    else if (str_startswith(command, "rename")) {
        char* file = command + 7;
        int firstarg_length = 0;
        char first_arg[64];
        char second_arg[64];

        if (!fs_exists(file)) {
            dogeio_text_println("Error: file doesn't exist");
            dogeio_text_println("Could be a typo.");
            handled = -1;
        } else {
            for (int i = 0; i < (int)str_strlen(file); i++) {

                if (file[i] != ' ') {
                    firstarg_length++;
                } else {
                    str_strncpy(first_arg, file, (size_t)firstarg_length);
                    str_strcpy(second_arg, file + firstarg_length);
                }

            }

            fs_rename(first_arg, second_arg);
            handled = 0;
        }
    }

    // create a file
    else if (str_startswith(command, "create")) {
        char* file = command + 7;
        if (fs_create(file) == -1) {
            dogeio_text_println("Much Sad: something went a little oops");
        }

        handled = 0;   
    }

    // delete a file
    else if (str_startswith(command, "del")) {
        char* file = command + 4;
        if (!fs_exists(file)) {
            dogeio_text_println("Error; file doesn't exist, could be a typo");
            handled = -1;
        } else {
            fs_delete(file);
            handled = 0;
        }
    }

    // whereami
    else if (str_strcmp(command, "whereami") == 0) {
        dogeio_text_println(fs_dirname());
        handled = 0;
    }

    /*  system information  */
    else if (str_strcmp(command, "whoami") == 0) {
        dogeio_text_println(current_user);
        handled = 0;
    }

    else if (str_strcmp(command, "cpuinfo") == 0) {
        dogeio_text_println(cpuid());
        handled = 0;
    }

    else if (str_strcmp(command, "fetch") == 0) {
        system_fetch();
        handled = 0;
    }

    else if (str_strcmp(command, "date") == 0) {
        dogeio_text_print(date_get());
        dogeio_text_print(" ");
        dogeio_text_println(time_get());
        handled = 0;
    }

    else if (str_strcmp(command, "time") == 0) {
        dogeio_text_println(time_get());
        handled = 0;
    }

    /*  system utilities  */
    else if (str_startswith(command, "edit")) {
        char* filename = command + 5;
        while (*filename == ' ') filename++;

        size_t len = str_strlen(filename);
        while (len > 0 && (filename[len - 1] == '\n' || filename[len - 1] == '\r' || filename[len - 1] == ' ')) {
            filename[len - 1] = '\0';
            len--;
        }

        if (str_strlen(filename) == 0) {
            dogeio_text_println("Error: no filename specified :(");
        } else {
            if (!fs_exists(filename)) {
                fs_create(filename);
            }
            system_editor(filename);
        }
        handled = 0;
    }

    else if (str_startswith(command, "dogescript")) {
        char* filename = command + 11;

        if (fs_exists(filename)) {
            static char* dogescript_buffer[4096];
            int bytes_read = fs_read(filename, buffer, 8192);
            int last_free = 0;
            int increment = 0;
            int location  = 0;
            int lines     = 0;

            for (int i = 0; i < (int)str_strlen(buffer); i++) {
                if (buffer[i] == '\n') {
                    str_strncpy(dogescript_buffer[last_free], buffer + location, increment);
                    location = location + increment;
                    lines++;
                } else if (buffer[i] == '\0') {
                    str_strncpy(dogescript_buffer[last_free], buffer + location, increment);
                    lines++;
                } else {
                    increment++;
                }
            }

            for (int i = 0; i < lines; i++) {
                system_dogescript_execute(dogescript_buffer[i]);
            }
            handled = 0;
        } else {
            dogeio_text_println("error: file doesn't exist :(");
            handled = -1;
        }
   }

    // other stuff
    str_strcpy(old2, (const char*)fs_dirname);
    fs_chdir(user);
    fs_write(".history", command);
    fs_chdir(old2);

    if (handled == 1) {
        dogeio_text_print(command);
        dogeio_text_println(": command not fonund :(");
    }

    return handled;
}

void system_dogeshell(void) {
    char input[256];
    int status = 0;
    str_strcpy(user, "/users/");
    str_strcat(user, current_user);

    fs_chdir(user);
    if (!fs_exists(".history")) {
        fs_create(".history");
    }

    while (true) {
        dogeio_text_color_change(0xFF00FF00);
        dogeio_text_print(current_user);
        dogeio_text_color_change(saved_color);
        dogeio_text_print(" (");

        char home_path[128];
        for (int i = 0; i < 128; i++) {
            home_path[i] = '\0';
        }
        
        str_strcpy(home_path, "/users/");
        str_strcat(home_path, current_user);

        char* current_dir = fs_dirname();

        if (str_strcmp(current_dir, home_path) == 0) {
            dogeio_text_print("~");
        } else {
            dogeio_text_print(current_dir);
        }

        dogeio_text_print(") ");

        if (status != 0) {
            static char code_buffer[4];
            str_itoa(status, code_buffer);
            dogeio_text_color_change(0xFFFF0000);
            dogeio_text_print("[");
            dogeio_text_print(code_buffer);
            dogeio_text_print("] ");
        }

        dogeio_text_color_change(saved_color);
        dogeio_text_input("> ", input, 256);
        status = system_dogeshell_ex(input);
    }
}
