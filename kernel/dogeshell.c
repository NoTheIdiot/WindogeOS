#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <basicutil.h>
#include <system.h>
#include <time.h>
#include <image.h>

uint32_t old = 0xFFCCCCCC;
extern void fstest_shell();

static void append_history(const char* command) {
    if (!fs_exists(".history")) {
        fs_create(".history");
    }
    fs_write(".history", (char*)command);
}

int system_dogeshell_ex(char* command) {
    int handled = 1; 
    
    char* help_command_array[20] = {
        "print              | simply prints a piece of text",
        "clear              | clears the terminal screen",
        "dir / list-directory| list contents of current folder",
        "read / read-file   | outputs the contents of a file",
        "write / write-file | writes a str of text to a file",
        "help               | outputs this help menu breakdown",
        "shutdown           | shutsdown the computer",
        "cpuinfo            | prints the cpu name",
        "del / delete-file  | deletes a file",
        "create / create-file| creates a file",
        "rename / rename-file| renames a file",
        "whoami             | displays your current username",
        "whereami           | displays your current folder location",
        "time               | displays the time",
        "ver                | shows version",
        "fetch              | shows the system information",
        "color              | changes color of text.",
        "history            | show command history from .history",
        "clear-history      | clears history, saves space.",
        "edit               | shows the very basic code editor"
    };

    if (command == NULL || command[0] == '\0') {
        return 0; 
    }

    if (str_strcmp(command, "print") == 0 || str_startswith(command, "print ")) {
        size_t len = str_strlen(command);
        if (len >= 6 && command[5] == ' ') {
            dogeio_text_println(command + 6);
        } else {
            dogeio_text_println(""); 
        }
        handled = 0;
    }

    else if (str_startswith(command, "color")) {
        const char *arg = command + 6;
        if (str_strcmp(arg, "black") == 0) {
            dogeio_text_color = 0xFF000000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "white") == 0) {
            dogeio_text_color = 0xFFFFFFFF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "grey") == 0) {
            dogeio_text_color = 0xFF808080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "dark_grey") == 0) {
            dogeio_text_color = 0xFF404040;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "red") == 0) {
            dogeio_text_color = 0xFFFF0000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "green") == 0) {
            dogeio_text_color = 0xFF00FF00;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "blue") == 0) {
            dogeio_text_color = 0xFF0000FF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "yellow") == 0) {
            dogeio_text_color = 0xFFFFFF00;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "cyan") == 0) {
            dogeio_text_color = 0xFF00FFFF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "magenta") == 0) {
            dogeio_text_color = 0xFFFF00FF;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "navy") == 0) {
            dogeio_text_color = 0xFF000080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "maroon") == 0) {
            dogeio_text_color = 0xFF800000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "teal") == 0) {
            dogeio_text_color = 0xFF008080;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "olive") == 0) {
            dogeio_text_color = 0xFF808000;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "doge_gold") == 0) {
            dogeio_text_color = 0xFFE1B857;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "doge_tan") == 0) {
            dogeio_text_color = 0xFFF4DFB1;
            dogeio_text_clear();
        } else if (str_strcmp(arg, "gray") == 0) {
            dogeio_text_color = 0xFFCCCCCC;
            dogeio_text_clear();
        } else {
            dogeio_text_println("unknown colorpreset.");
        }
        old = dogeio_text_color;
        handled = 0;
    }

    else if (str_strcmp(command, "shutdown") == 0) {
        dogeio_text_clear_raw();
        dogeio_text_println("Such shutdown, very goodbye.");
        halt();
        handled = 0;
    }

    else if (str_startswith(command, "cd")) {
        int result = fs_chdir(command + 3);
        if (result == -2) {
            dogeio_text_println("Much Error: Not a Folder.");
        } else if (result == -1) {
            dogeio_text_println("Such Error: Folder not existing :(");
        }
        handled = 0;
    }

    else if (str_startswith(command, "mkdir") || str_startswith(command, "make-folder")) {
        char* argument = str_startswith(command, "make-folder") ? (command + 12) : (command + 6);
        fs_mkdir(argument);
        handled = 0;
    }

    // to test 
    else if (str_strcmp(command, "testimage") == 0) {
        system_parse_tga("image.tga");
        handled = 0;
    }

    else if (str_strcmp(command, "format") == 0) {
        char r_u_sure[4];
        dogeio_text_input("Are You Sure? (yes/no)\nMUCH WARNING: THIS WILL ERASE THE DISK.\n", r_u_sure, 4);
        if (str_strcmp(r_u_sure, "yes") == 0) {
            int result = fs_format();
            if (result) {
                dogeio_text_println("Formated Disk.");
            } else {
                dogeio_text_println("Not Wow: Something Went Wrong.");
            }
        } else {
            dogeio_text_println("Format skipped.");
        }
        handled = 0;
    }

    else if (str_strcmp(command, "clear") == 0) {
        dogeio_text_clear();
        handled = 0;
    }

    else if (str_startswith(command, "dir") || str_startswith(command, "list-directory")) {
        char* argument = str_startswith(command, "list-directory") ? (command + 14) : (command + 3);
        while (*argument == ' ') argument++;
        
        if (str_strcmp(argument, "--showhidden") == 0 || str_strcmp(argument, "-sh") == 0) {
            fs_list_dir(1);
        } else {
            fs_list_dir(0);
        }
        handled = 0;
    }

    else if (str_startswith(command, "read") || str_startswith(command, "read-file")) {
        char* filename = str_startswith(command, "read-file") ? (command + 9) : (command + 4);
        while (*filename == ' ') filename++;

        static char output_buffer[8192];
        int bytes_read = fs_read(filename, output_buffer, sizeof(output_buffer));
        if (bytes_read < 0) {
            dogeio_text_println("Error: unable to read file.");
        } else {
            char *line = output_buffer;
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

    else if (str_startswith(command, "write") || str_startswith(command, "write-file")) {
        char* filename = str_startswith(command, "write-file") ? (command + 10) : (command + 5);
        while (*filename == ' ') filename++;

        static char text[256];
        dogeio_text_input("text> ", text, 256);
        
        int result = fs_write(filename, text);
        if (result == 1) {
            dogeio_text_println("write ok");
        } else if (result == 0) {
            dogeio_text_println("Error: disk full.");
        } else if (result == -2) {
            dogeio_text_println("Error: file not found.");
        } else {
            dogeio_text_println("Not Wow: Something went wrong.");
        }
        handled = 0;
    }

    else if (str_startswith(command, "create") || str_startswith(command, "create-file")) {
        char* filename = str_startswith(command, "create-file") ? (command + 11) : (command + 6);
        while (*filename == ' ') filename++;

        int result = fs_create(filename);
        if (!result) {
            dogeio_text_println("Not Wow: Failed to create file.");
            dogeio_text_println("File name potentially invalid: nothing cant be a filename.");
        }
        handled = 0;
    }

    else if (str_startswith(command, "del") || str_startswith(command, "delete-file")) {
        char* filename = str_startswith(command, "delete-file") ? (command + 11) : (command + 3);
        while (*filename == ' ') filename++;

        int result = fs_delete(filename);
        if (!result) {
            dogeio_text_println("Not Wow: File Not Found.");
        } else if (result == -1) {
            dogeio_text_println("Doge Sad: Something went wrong!");
        } else if (result == -2) {
            dogeio_text_println("Error: File does not exist.");
        }
        handled = 0;
    }

    else if (str_startswith(command, "rename") || str_startswith(command, "rename-file")) {
        char* filename = str_startswith(command, "rename-file") ? (command + 11) : (command + 6);
        while (*filename == ' ') filename++;

        char new_name[32];
        dogeio_text_input("new filename> ", new_name, 32);
        fs_rename(filename, new_name);
        handled = 0;
    }

    else if (str_startswith(command, "time")) {
        dogeio_text_println(time_get());
        handled = 0;
    }

    else if (str_startswith(command, "edit")) {
        char* filename = command + 4;
        while (*filename == ' ') filename++;

        size_t len = str_strlen(filename);
        while (len > 0 && (filename[len - 1] == '\n' || filename[len - 1] == '\r' || filename[len - 1] == ' ')) {
            filename[len - 1] = '\0';
            len--;
        }

        if (!fs_exists(filename)) {
            fs_create(filename);
        }

        if (str_strlen(filename) == 0) {
            dogeio_text_println("Error: No filename specified.");
        } else {
            editor(filename);
        }
        handled = 0;
    }

    else if (str_strcmp(command, "history") == 0) {
        static char output_buffer[8192];
        int bytes_read = fs_read(".history", output_buffer, sizeof(output_buffer));
        if (bytes_read < 0) {
            dogeio_text_println("No history available.");
        } else {
            char *line = output_buffer;
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
        for (int i = 0; i < 20; i++) {
            dogeio_text_println(help_command_array[i]);
        }
        handled = 0;
    }

    else if (str_startswith(command, "cpuinfo")) {
        dogeio_text_println("Cpu Name: ");
        dogeio_text_println(cpuid());
        handled = 0;
    }

    else if (str_strcmp(command, "clear-history") == 0) {
        fs_delete(".history");
        fs_create(".history");
        handled = 0;
    }

    else if (str_strcmp(command, "fetch") == 0) {
        system_fetch();
        handled = 0;
    }

    else if (str_startswith(command, "raminfo")) {
        dogeio_text_print("RAM Amount: ");
        handled = 0;
    }

    else if (str_strcmp(command, "bash") == 0) {
        system_bash();
        handled = 0;
    }

    else if (str_strcmp(command, "ver") == 0) {
        dogeio_text_println(windoge_version);
        handled = 0;
    }

    else if (str_strcmp(command, "whoami") == 0) {
        dogeio_text_println("wow");
        handled = 0;
    }
    
    else if (str_strcmp(command, "whereami") == 0) {
        dogeio_text_println("root (/)");
        handled = 0;
    }

    if (handled == 1) {
        dogeio_text_print(command);
        dogeio_text_println(": command doesn't exist :(");
    }

    return handled;
}

void system_dogeshell() {
    char input[256];
    int status = 0;

    if (!fs_exists(".history")) {
        fs_create(".history");
    }

    while (true) {
        dogeio_text_color_change(0xFF00FF00);
        dogeio_text_print("wow");
        dogeio_text_color_change(old);
        dogeio_text_print(" (");
        dogeio_text_print(fs_dirname());
        dogeio_text_print(") ");

        if (status == 1) {
            dogeio_text_color_change(0xFFFF0000);
            dogeio_text_print("[1] ");
        }

        dogeio_text_color_change(old);
        dogeio_text_input("> ", input, 256);
        if (input[0] != '\0') {
            append_history(input);
        }
        status = system_dogeshell_ex(input);
    }
}
