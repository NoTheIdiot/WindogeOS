/*
Settings. Thats it.
*/

#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <bool.h> 

extern uint32_t old; 

void system_settings() {

    char input[4];
    char color_input[16];
    char default_shell[16];

    dogeio_text_clear();
    while (true) {
        dogeio_text_println("WindogeOS Settings");
        dogeio_text_println("[1] Color");
        dogeio_text_println("[2] Default Shell");
        dogeio_text_println("[3] Exit");

        dogeio_text_input("> ", input, 4);

        if (str_strcmp(input, "1") == 0) {
            bool valid_color = false;

            while (!valid_color) {
                dogeio_text_println("Choose Such Color.");
                dogeio_text_println("Type 'back' to return.");
                dogeio_text_input("> ", color_input, 16);
                
                const char *arg = color_input;

                if (str_strcmp(arg, "back") == 0) {
                    break;
                }

                valid_color = true; 

                if (str_strcmp(arg, "black") == 0) {
                    dogeio_text_color = 0xFF000000;
                } else if (str_strcmp(arg, "white") == 0) {
                    dogeio_text_color = 0xFFFFFFFF;
                } else if (str_strcmp(arg, "grey") == 0) {
                    dogeio_text_color = 0xFF808080;
                } else if (str_strcmp(arg, "dark_grey") == 0) {
                    dogeio_text_color = 0xFF404040;
                } else if (str_strcmp(arg, "red") == 0) {
                    dogeio_text_color = 0xFFFF0000;
                } else if (str_strcmp(arg, "green") == 0) {
                    dogeio_text_color = 0xFF00FF00;
                } else if (str_strcmp(arg, "blue") == 0) {
                    dogeio_text_color = 0xFF0000FF;
                } else if (str_strcmp(arg, "yellow") == 0) {
                    dogeio_text_color = 0xFFFFFF00;
                } else if (str_strcmp(arg, "cyan") == 0) {
                    dogeio_text_color = 0xFF00FFFF;
                } else if (str_strcmp(arg, "magenta") == 0) {
                    dogeio_text_color = 0xFFFF00FF;
                } else if (str_strcmp(arg, "navy") == 0) {
                    dogeio_text_color = 0xFF000080;
                } else if (str_strcmp(arg, "maroon") == 0) {
                    dogeio_text_color = 0xFF800000;
                } else if (str_strcmp(arg, "teal") == 0) {
                    dogeio_text_color = 0xFF008080;
                } else if (str_strcmp(arg, "olive") == 0) {
                    dogeio_text_color = 0xFF808000;
                } else if (str_strcmp(arg, "doge_gold") == 0) {
                    dogeio_text_color = 0xFFE1B857;
                } else if (str_strcmp(arg, "doge_tan") == 0) {
                    dogeio_text_color = 0xFFF4DFB1;
                } else {
                    dogeio_text_println("unknown colorpreset.");
                    valid_color = false; 
                }

                if (valid_color) {
                    fs_delete_last_line(".settings_color");
                    fs_write(".settings_color", (char *)arg);
                    old = dogeio_text_color;
                }
            }

            dogeio_text_clear(); 
        }

        else if (str_strcmp(input, "2") == 0) {
            bool valid_shell = false;

            while (!valid_shell) {
                dogeio_text_println("Choose default shell.");
                dogeio_text_println("[1] dogeshell");
                dogeio_text_println("[2] sbash (small bash)");
                dogeio_text_println("[3] back");
                dogeio_text_input("> ", default_shell, 16);

                const char *shell_arg = default_shell;

                if (str_strcmp(shell_arg, "3") == 0 || str_strcmp(shell_arg, "back") == 0) {
                    break;
                }

                if (str_strcmp(shell_arg, "1") == 0 || str_strcmp(shell_arg, "dogeshell") == 0) {
                    valid_shell = true;
                    shell_arg = "dogeshell";
                } else if (str_strcmp(shell_arg, "2") == 0 || str_strcmp(shell_arg, "sbash") == 0) {
                    valid_shell = true;
                    shell_arg = "sbash";
                } else {
                    dogeio_text_println("unknown shell configuration.");
                    valid_shell = false;
                }

                if (valid_shell) {
                    fs_delete_last_line(".settings");
                    fs_write(".settings", (char *)shell_arg);
                }
            }
            dogeio_text_clear(); 
        }

        else if (str_strcmp(input, "3") == 0) {
            return;
        }

        else {
            dogeio_text_clear(); 
        }
    }
}
