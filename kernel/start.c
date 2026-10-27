#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <system.h>
#include <sysinfo.h>
#include <start.h>
#include <basicutil.h>

extern int default_shell_is_bash;

void start_setup(void) {
    fs_mount();
    fs_set_auth_override(1);

    char proceed[1];
    dogeio_color_change(0xE1B16C);
    for (int i = 0; i < 24; i++) {
        dogeio_print(" ");
        dogeio_println(doge_ascii_full[i]);
    }
    dogeio_color_change(COLOR_GRAY);
    dogeio_println(" Welcome to WindogeOS setup! Such doge, very OS.");
    dogeio_input(" Press enter to start setup.", proceed, 1);

    log("\n[dogeing] creating folders");
    if (!fs_exists("/system")) {
        fs_mkdir("/system");
    }
    if (!fs_exists("/system/accounts")) {
        fs_mkdir("/system/accounts");
    }
    if (!fs_exists("/users")) {
        fs_mkdir("/users");
    }
    if (!fs_exists("/apps")) {
        fs_mkdir("/apps");
        fs_chdir("apps");
        fs_mkdir("packages");
        fs_chdir("/");
    }

    char username_new[64];
    char password_new[64];
    char root_new[64];

    while (true) {
        dogeio_clear();
        dogeio_color_change(0xE1B16C);
        for (int i = 0; i < 24; i++) {
            dogeio_print(" ");
            dogeio_println(doge_ascii_full[i]);
        }
        
        dogeio_color_change(COLOR_GRAY);
        dogeio_println(" ===== ACCOUNT CREATION ===== ");
        dogeio_println("\n Set the admin password. ");
        dogeio_input(" [admin]> ", root_new, 64);
        clean_input_string(root_new);
        if (root_new[0] == '\0') {
            dogeio_println(" Password cannot be empty.");
            dogeio_get_key();
            continue;
        }
        break;
    }

    system_create_user("admin", root_new, 0);

    while (true) {
        dogeio_clear();
        dogeio_color_change(0xE1B16C);
        for (int i = 0; i < 24; i++) {
            dogeio_print(" ");
            dogeio_println(doge_ascii_full[i]);
        }
        
        dogeio_color_change(COLOR_GRAY);
        dogeio_println(" ===== ACCOUNT CREATION ===== ");
        dogeio_println("\n Create system username and password.");
        dogeio_input(" [username]> ", username_new, 64);
        clean_input_string(username_new);

        if (username_new[0] == '\0') {
            dogeio_println("Username cannot be empty.");
            dogeio_println(" Press Enter to continue...");
            dogeio_get_key();
            continue;
        }

        if (str_strcmp(username_new, "admin") == 0) {
            dogeio_println(" Username cannot be 'admin'.");
            dogeio_println(" Press Enter to continue...");
            dogeio_get_key();
            continue;
        }
        
        dogeio_input(" [password]> ", password_new, 64);
        clean_input_string(password_new);
        if (password_new[0] == '\0') {
            dogeio_println(" Password cannot be empty.");
            dogeio_println(" Press Enter to continue...");
            dogeio_get_key();
            continue;
        }
        break;
    }

    while (true) {
        dogeio_clear();
        dogeio_color_change(0xE1B16C);
        for (int i = 0; i < 24; i++) {
            dogeio_print(" ");
            dogeio_println(doge_ascii_full[i]);
        }
        dogeio_color_change(COLOR_GRAY);
        dogeio_println(" ===== ACCOUNT CREATION ===== ");
        dogeio_println(" Name your computer (old or new)");
        dogeio_input("\n [name]> ", computer_name, 64);
        if (computer_name[0] == '\0') {
            dogeio_println(" Computer name cannot be empty.");
            dogeio_println(" Press Enter to continue...");
            dogeio_get_key();
            continue;
        }
        break;
    }

    log("[dogeing] creating accounts");
    system_create_user(username_new, password_new, 1);
    
    dogeio_clear();
    dogeio_color_change(0xE1B16C);
    for (int i = 0; i < 24; i++) {
        dogeio_print(" ");
        dogeio_println(doge_ascii_full[i]);
    }
    dogeio_color_change(COLOR_GRAY);
    dogeio_println(" ===== USER PREFERENCES =====");

    dogeio_println(" Select Default Font:");
    dogeio_println(" 1. PC IBM (Default)");
    dogeio_println(" 2. Ubuntu Mono");
    dogeio_println(" 3. Terminus");
    dogeio_println(" 4. Tamzen");
    dogeio_println(" 5. Spleen");
    char font_choice[8];
    dogeio_input(" Choice [1-5, default 1]: ", font_choice, sizeof(font_choice));
    const char *font_val = "pc_ibm";
    if      (font_choice[0] == '2') font_val = "ubuntu_mono";
    else if (font_choice[0] == '3') font_val = "terminus";
    else if (font_choice[0] == '4') font_val = "tamzen";
    else if (font_choice[0] == '5') font_val = "spleen";

    dogeio_clear();
    dogeio_color_change(0xE1B16C);
    for (int i = 0; i < 24; i++) {
        dogeio_print(" ");
        dogeio_println(doge_ascii_full[i]);
    }
    dogeio_color_change(COLOR_GRAY);
    dogeio_println(" ===== USER PREFERENCES ===== ");
    dogeio_println("\n Select Default Shell:");
    dogeio_println(" 1. Dogeshell (Default)");
    dogeio_println(" 2. Bash");
    char shell_choice[8];
    dogeio_input(" Choice [1-2, default 1]: ", shell_choice, sizeof(shell_choice));
    const char *shell_val = (shell_choice[0] == '2') ? "bash" : "dogeshell";

    dogeio_clear();
    dogeio_color_change(0xE1B16C);
    for (int i = 0; i < 24; i++) {
        dogeio_print(" ");
        dogeio_println(doge_ascii_full[i]);
    }
    dogeio_color_change(COLOR_GRAY);
    dogeio_println(" ===== USER PREFERENCES ===== ");
    dogeio_println("\n Select Default Text Color:");
    dogeio_println(" 1. Gray (Default)");
    dogeio_println(" 2. White");
    dogeio_println(" 3. Green");
    dogeio_println(" 4. Cyan");
    char color_choice[8];
    dogeio_input(" Choice [1-4, default 1]: ", color_choice, sizeof(color_choice));
    const char *color_val = "gray";
    if      (color_choice[0] == '2') color_val = "white";
    else if (color_choice[0] == '3') color_val = "green";
    else if (color_choice[0] == '4') color_val = "cyan";

    log("[dogeing] creating boot configuration file (.windoge)");
    if (fs_create(".windoge") == 0) {
        char config_buf[128];
        str_strcpy(config_buf, "text_color=");
        str_strcat(config_buf, color_val);
        str_strcat(config_buf, "\nshell=");
        str_strcat(config_buf, shell_val);
        str_strcat(config_buf, "\nfont=");
        str_strcat(config_buf, font_val);
        str_strcat(config_buf, "\n");
        fs_write(".windoge", config_buf);
    }

    log("[wow] setup complete");
    log("[dogeing] prompting for reboot");

    dogeio_clear();
    dogeio_color_change(0xE1B16C);
    for (int i = 0; i < 24; i++) {
        dogeio_print(" ");
        dogeio_println(doge_ascii_full[i]);
    }
    dogeio_color_change(COLOR_GRAY);
    char nothing[1];
    dogeio_input(" Press enter to reboot... ", nothing, 1);
    fs_set_auth_override(0);
    core_reboot();
}

void start_shell(void) {
    if (default_shell_is_bash) {
        system_bash();
    } else {
        system_dogeshell();
    }
}