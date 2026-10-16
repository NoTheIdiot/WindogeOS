#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/dogelib.h"
#include "../library/stdint.h"
#include "../library/stddef.h"

#define MAX_ITEMS 8
#define PATH_CAPACITY 512
#define MAX_VISIBLE_ROWS 15

void _start(void) {
    int selected = 0;
    uint64_t count = 0;

    char file_names[MAX_ITEMS + 4][256];
    char cwd[PATH_CAPACITY];

    background_color(COLOR_BACKGROUND);
    text_color(COLOR_WHITE);
    clear();

    while (1) {
        memset(cwd, 0, PATH_CAPACITY);
        get_cwd(cwd, PATH_CAPACITY);
        cwd[PATH_CAPACITY - 1] = '\0';

        memset(file_names, 0, sizeof(file_names));
        count = list_dir(file_names, MAX_ITEMS);
        
        if (count > MAX_ITEMS + 4) {
            count = MAX_ITEMS + 4;
        }

        clear();
        print_at("=== DOGE TUI FILE MANAGER ===", 2, 1, COLOR_BRIGHT_ORANGE);
        print_at("CWD: ", 2, 2, COLOR_GRAY);
        print_at(cwd, 7, 2, COLOR_BRIGHT_CYAN);
        print_at("-------------------------------------------", 2, 3, COLOR_DARK_GRAY);

        if (count == 0) {
            selected = 0;
            print_at("[ Empty Directory ]", 4, 5, COLOR_BRIGHT_BLACK);
        } else {
            if (selected >= (int)count) {
                selected = (int)count - 1;
            }
            if (selected < 0) {
                selected = 0;
            }

            for (uint64_t i = 0; i < count; i++) {
                file_names[i][sizeof(file_names[i]) - 1] = '\0';

                uint32_t current_y = 5 + (uint32_t)i;
                
                if (current_y > 5 + MAX_VISIBLE_ROWS) {
                    break;
                }

                uint32_t text_color_to_use = COLOR_WHITE;

                dogec_stat_t file_stat;
                memset(&file_stat, 0, sizeof(dogec_stat_t));
                stat(file_names[i], &file_stat);

                if (file_stat.is_dir) {
                    text_color_to_use = COLOR_BRIGHT_BLUE;
                }

                if ((int)i == selected) {
                    print_at("> ", 2, current_y, COLOR_CURSOR);
                    print_at(file_names[i], 4, current_y, COLOR_BRIGHT_YELLOW);
                    if (file_stat.is_dir) {
                        print_at("/", 4 + (uint32_t)str_strlen(file_names[i]), current_y, COLOR_BRIGHT_BLUE);
                    }
                } else {
                    print_at("  ", 2, current_y, COLOR_BACKGROUND);
                    print_at(file_names[i], 4, current_y, text_color_to_use);
                    if (file_stat.is_dir) {
                        print_at("/", 4 + (uint32_t)str_strlen(file_names[i]), current_y, COLOR_DARK_BLUE);
                    }
                }
            }
        }

        uint32_t footer_y = 6 + (uint32_t)(count < MAX_VISIBLE_ROWS ? count : MAX_VISIBLE_ROWS);
        if (footer_y < 20) footer_y = 20;
        if (footer_y > 23) footer_y = 23;

        print_at("-------------------------------------------", 2, footer_y, COLOR_DARK_GRAY);
        print_at("[W/S] Navigate | [Enter] Open Dir | [Q] Exit", 2, footer_y + 1, COLOR_BRIGHT_GREEN);

        uint64_t key = get_key();

        if (key == 'w' || key == 'W') {
            if (selected > 0) selected--;
        } 
        else if (key == 's' || key == 'S') {
            if (selected < (int)count - 1) selected++;
        } 
        else if (key == '\n' || key == '\r' || key == 13) {
            if (count > 0) {
                dogec_stat_t target_stat;
                memset(&target_stat, 0, sizeof(dogec_stat_t));
                stat(file_names[selected], &target_stat);

                if (target_stat.is_dir) {
                    change_dir(file_names[selected]);
                    selected = 0;
                }
            }
        } 
        else if (key == 'q' || key == 'Q') {
            clear();
            sys_exit((uint64_t)0);
        }
    }
}