#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <system.h>
#include <sysinfo.h>
#include <basicutil.h>
#include <math.h>

static char settings_path[] = "/.windoge";
static size_t selected_color;
int default_shell_is_bash;
static size_t selected_font_idx = 3;
extern uint32_t saved_color;
extern void dogeshell_list_pci(void);

static const settings_color_t settings_colors[] = {
    {"Gray", "gray", 0xFFCCCCCC},
    {"White", "white", 0xFFFFFFFF},
    {"Red", "red", 0xFFFF5555},
    {"Green", "green", 0xFF55FF55},
    {"Yellow", "yellow", 0xFFFFED29},
    {"Blue", "blue", 0xFF5555FF},
    {"Magenta", "magenta", 0xFFFF55FF},
    {"Cyan", "cyan", 0xFF55FFFF},
};

static const settings_font_t settings_fonts[] = {
    {"Ubuntu Mono", "ubuntu_mono", ubuntu_mono},
    {"Terminus",    "terminus",    terminus},
    {"Tamzen",      "tamzen",      tamzen},
    {"PC IBM",      "pc_ibm",      vga_text},
    {"Spleen",      "spleen",      vga_text},
};

static void settings_draw_menu(const char *title, const char *const *items,
                             size_t item_count, size_t selected) {
    dogeio_clear();
    dogeio_println(title);
    dogeio_println("");
    for (size_t i = 0; i < item_count; i++) {
        dogeio_print(i == selected ? "  > " : "    ");
        dogeio_println(items[i]);
    }
    dogeio_println("");
    dogeio_println("Use Up/Down and Enter. Backspace returns.");
}

static inline void settings_load_value(const char *value) {
    for (size_t i = 0; i < sizeof(settings_colors) / sizeof(settings_colors[0]); i++) {
        if (str_strcmp(value, settings_colors[i].value) == 0) {
            selected_color = i;
            saved_color = settings_colors[i].color;
            return;
        }
    }
}

static inline void settings_load_font_value(const char *value) {
    for (size_t i = 0; i < sizeof(settings_fonts) / sizeof(settings_fonts[0]); i++) {
        if (str_strcmp(value, settings_fonts[i].value) == 0) {
            selected_font_idx = i;
            dogeio_set_font(settings_fonts[i].font_ptr);
            return;
        }
    }
}

void system_load_settings(void) {
    char contents[128];
    int bytes_read;

    selected_color = 0;
    saved_color = settings_colors[0].color;
    default_shell_is_bash = 0;
    selected_font_idx = 3;
    dogeio_set_font(settings_fonts[selected_font_idx].font_ptr);

    if (!fs_exists(settings_path)) {
        return;
    }

    bytes_read = fs_read(settings_path, contents, sizeof(contents) - 1);
    if (bytes_read < 0) {
        dogeio_println("Settings error: unable to read /.windoge.");
        dogeio_color_change(saved_color);
        return;
    }
    contents[bytes_read] = '\0';

    char *line = contents;
    while (*line != '\0') {
        char *line_end = line;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }
        char line_buffer[32];
        size_t line_length = (size_t)(line_end - line);
        if (line_length < sizeof(line_buffer)) {
            str_strncpy(line_buffer, line, line_length + 1);
            line_buffer[line_length] = '\0';
            if (str_startswith(line_buffer, "text_color=")) {
                settings_load_value(line_buffer + 11);
            } else if (str_strcmp(line_buffer, "shell=bash") == 0) {
                default_shell_is_bash = 1;
            } else if (str_strcmp(line_buffer, "shell=dogeshell") == 0) {
                default_shell_is_bash = 0;
            } else if (str_startswith(line_buffer, "font=")) {
                settings_load_font_value(line_buffer + 5);
            }
        }
        line = line_end;
        while (*line == '\n' || *line == '\r') {
            line++;
        }
    }
    dogeio_color_change(saved_color);
}

static inline int settings_save(void) {
    char contents[128] = "text_color=";
    str_strcat(contents, settings_colors[selected_color].value);
    str_strcat(contents, "\nshell=");
    str_strcat(contents, default_shell_is_bash ? "bash\n" : "dogeshell\n");
    str_strcat(contents, "font=");
    str_strcat(contents, settings_fonts[selected_font_idx].value);
    str_strcat(contents, "\n");

    if (!fs_exists(settings_path) &&
        fs_create(settings_path) != 0) {
        return -1;
    }
    return fs_write(settings_path, contents);
}

static void settings_show_system_information(void) {
    dogeio_clear();
    dogeio_println("[ System Information ]\n");
    dogeio_print("Version: ");
    dogeio_println(windoge_version);
    dogeio_print("CPU: ");
    dogeio_println(cpuid_string());
    dogeio_print("RAM: ");
    char ram_used_str[32];
                
    uint64_t ram_total_mb = get_ram();

    str_kutoa(ram_total_mb, ram_used_str);
    dogeio_print(ram_used_str);
    dogeio_println(" MB");
    char width[32];
    char height[32];

    dogeio_print("Display: ");

    str_itoa(dogeio_screen_width(), width);
    str_itoa(dogeio_screen_height(), height);

    dogeio_print(width);
    dogeio_print("x");
    dogeio_println(height);
    dogeio_println("\nPress any key to return...");
    dogeio_get_key();
}

static void settings_draw_color_menu(size_t selected) {
    size_t color_count = sizeof(settings_colors) / sizeof(settings_colors[0]);
    dogeio_clear();
    dogeio_println("[ Default Text Color ]");
    dogeio_println("");
    for (size_t i = 0; i < color_count; i++) {
        dogeio_print(i == selected ? "  > " : "    ");
        dogeio_println(settings_colors[i].label);
    }
    dogeio_println("");
    dogeio_println("Use Up/Down and Enter. Backspace returns.");
}

static void settings_select_color(void) {
    size_t selected = selected_color;
    size_t color_count = sizeof(settings_colors) / sizeof(settings_colors[0]);

    while (true) {
        settings_draw_color_menu(selected);
        uint16_t key = dogeio_get_key();
        switch (key) {
        case KEY_UP:
            selected = selected == 0 ? color_count - 1 : selected - 1;
            break;
        case KEY_DOWN:
            selected = (selected + 1) % color_count;
            break;
        case KEY_BACKSPACE:
        case (uint16_t)'q':
            return;
        case KEY_ENTER:
            selected_color = selected;
            saved_color = settings_colors[selected_color].color;
            dogeio_color_change(saved_color);
            if (settings_save() != 0) {
                dogeio_println("Unable to save settings to /.windoge.");
            } else {
                dogeio_println("Default text color saved.");
            }
            dogeio_get_key();
            return;
        }
    }
}

static void settings_select_shell(void) {
    static const char *shell_labels[] = {"Dogeshell", "Bash"};
    size_t selected = default_shell_is_bash ? 1 : 0;

    while (true) {
        settings_draw_menu("[ Default Shell ]", shell_labels,
                           sizeof(shell_labels) / sizeof(shell_labels[0]), selected);
        uint16_t key = dogeio_get_key();

        switch (key) {
        case KEY_UP:
        case KEY_DOWN:
            selected = selected == 0 ? 1 : 0;
            break;
        case KEY_BACKSPACE:
        case (uint16_t)'q':
            return;
        case KEY_ENTER:
            default_shell_is_bash = selected == 1;
            if (settings_save() != 0) {
                dogeio_println("Unable to save settings to /.windoge.");
            } else {
                dogeio_println("Default shell saved. It will be used after reboot.");
            }
            dogeio_get_key();
            return;
        }
    }
}

static void settings_select_font(void) {
    size_t font_count = sizeof(settings_fonts) / sizeof(settings_fonts[0]);
    size_t selected = selected_font_idx;
    
    const char *font_labels[5];
    for (size_t i = 0; i < font_count; i++) {
        font_labels[i] = settings_fonts[i].label;
    }

    while (true) {
        settings_draw_menu("[ Default Font ]", font_labels, font_count, selected);
        uint16_t key = dogeio_get_key();

        switch (key) {
        case KEY_UP:
            selected = selected == 0 ? font_count - 1 : selected - 1;
            break;
        case KEY_DOWN:
            selected = (selected + 1) % font_count;
            break;
        case KEY_BACKSPACE:
        case (uint16_t)'q':
            return;
        case KEY_ENTER:
            selected_font_idx = selected;
            dogeio_set_font(settings_fonts[selected_font_idx].font_ptr);
            if (settings_save() != 0) {
                dogeio_println("Unable to save settings to /.windoge.");
            } else {
                dogeio_println("Font settings saved.");
            }
            dogeio_get_key();
            return;
        }
    }
}

static void settings_run_test(void) {
    char* save = fs_dirname();
    fs_chdir("/");
    system_dogeshell_ex("run /apps/syscall_test.bin");
    fs_chdir(save);
}

static void clean_string(char* str) {
    if (!str) return;
    int len = 0;
    while (str[len] != '\0') len++;
    while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n' || str[len - 1] == ' ' || str[len - 1] == '\t')) {
        str[--len] = '\0';
    }
}

static void settings_user_accounts(void) {
    static const char *user_menu[] = {
        "Switch User / Login",
        "Create New User",
        "Back"
    };
    size_t selected = 0;

    while (true) {
        settings_draw_menu("[ User Accounts ]", user_menu, 3, selected);
        uint16_t key = dogeio_get_key();

        switch (key) {
        case KEY_UP:
            selected = selected == 0 ? 2 : selected - 1;
            break;
        case KEY_DOWN:
            selected = (selected + 1) % 3;
            break;
        case KEY_BACKSPACE:
        case (uint16_t)'q':
            return;
        case KEY_ENTER:
            if (selected == 0) {
                char login_user[64];
                char login_pass[64];
                dogeio_clear();
                dogeio_println("[ Switch User / Login ]\n");
                dogeio_input("Username: ", login_user, sizeof(login_user));
                clean_string(login_user);
                if (login_user[0] == '\0') {
                    dogeio_println("Username cannot be empty.");
                    dogeio_get_key();
                    break;
                }
                dogeio_input("Password: ", login_pass, sizeof(login_pass));
                clean_string(login_pass);

                if (system_verify_user(login_user, login_pass)) {
                    str_strcpy(current_user, login_user);
                    char userpath[128];
                    str_strcpy(userpath, "/users/");
                    str_strcat(userpath, login_user);
                    fs_chdir(userpath);
                    dogeio_print("\nSuccessfully logged in as ");
                    dogeio_println(login_user);
                } else {
                    dogeio_println("\nInvalid username or password.");
                }
                dogeio_get_key();
            } else if (selected == 1) {
                char new_user[64];
                char new_pass[64];
                dogeio_clear();
                dogeio_println("[ Create New User ]\n");
                dogeio_input("Enter username: ", new_user, sizeof(new_user));
                clean_string(new_user);
                if (new_user[0] == '\0') {
                    dogeio_println("Username cannot be empty.");
                    dogeio_get_key();
                    break;
                }
                dogeio_input("Enter password: ", new_pass, sizeof(new_pass));
                clean_string(new_pass);
                if (new_pass[0] == '\0') {
                    dogeio_println("Password cannot be empty.");
                    dogeio_get_key();
                    break;
                }
                if (system_create_user(new_user, new_pass, 1)) {
                    dogeio_println("\nUser created successfully!");
                } else {
                    dogeio_println("\nFailed to create user.");
                }
                dogeio_get_key();
            } else {
                return;
            }
            break;
        }
    }
}

void system_settings(void) {
    static const char *menu_items[] = {
        "System Information",
        "Default Text Color",
        "Default Shell",
        "PCI/PCIE devices",
        "Test System",
        "Default Font",
        "User Accounts",
        "Exit",
    };
    const size_t menu_count = sizeof(menu_items) / sizeof(menu_items[0]);
    size_t selected = 0;

    while (true) {
        settings_draw_menu("[ WindogeOS Settings ]", menu_items, menu_count, selected);
        uint16_t key = dogeio_get_key();
        switch (key) {
        case KEY_UP:
            selected = selected == 0 ? menu_count - 1 : selected - 1;
            break;
        case KEY_DOWN:
            selected = (selected + 1) % menu_count;
            break;
        case KEY_BACKSPACE:
        case (uint16_t)'q':
            dogeio_clear();
            dogeio_color_change(saved_color);
            return;
        case KEY_ENTER:
            switch (selected) {
            case 0:
                settings_show_system_information();
                break;
            case 1:
                settings_select_color();
                break;
            case 2:
                settings_select_shell();
                break;
            case 3:
                dogeio_clear();
                dogeshell_list_pci(); 
                dogeio_get_key();
                break;
            case 4:
                settings_run_test();
                break;
            case 5:
                settings_select_font();
                break;
            case 6:
                settings_user_accounts();
                break;
            case 7:
                dogeio_clear();
                dogeio_color_change(saved_color);
                return;
            default:
                dogeio_clear();
                dogeio_color_change(saved_color);
                return;
            }
        }
    }
}