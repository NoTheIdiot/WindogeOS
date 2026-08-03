/*
Settings. Thats it.
*/

#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <bool.h> 

void system_settings() {

    char input[4];
    char default_shell[16];

    dogeio_text_clear();
    while (true) {
        dogeio_text_println("WindogeOS Settings");
        dogeio_text_println("[1] Default Shell");
        dogeio_text_println("[2] Exit");

        dogeio_text_input("> ", input, 4);

        if (str_strcmp(input, "1") == 0) {
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

        else if (str_strcmp(input, "2") == 0) {
            return;
        }

        else {
            dogeio_text_clear(); 
        }
    }
}
