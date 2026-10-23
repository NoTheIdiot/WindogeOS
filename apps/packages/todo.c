#include "../library/dogeio.h"
#include "../library/bool.h"
#include "../library/string.h"
#include "../library/dogelib.h"

#define TODO_LEN 64

char** dynamic_todos = NULL;
bool* todo_done = NULL;
int total_todos = 0;
int todo_capacity = 0;
int selected_index = 0;

static void grow_capacity(void) {
    int new_capacity = (todo_capacity == 0) ? 4 : todo_capacity * 2;
    char** new_array = (char**)malloc(sizeof(char*) * (size_t)new_capacity);
    bool* new_done = (bool*)malloc(sizeof(bool) * (size_t)new_capacity);
    
    for (int i = 0; i < new_capacity; i++) {
        new_array[i] = (char*)malloc(sizeof(char) * TODO_LEN);
        for (int j = 0; j < TODO_LEN; j++) new_array[i][j] = '\0';
        new_done[i] = false;
    }

    if (dynamic_todos != NULL) {
        for (int i = 0; i < total_todos; i++) {
            str_strcpy(new_array[i], dynamic_todos[i]);
            new_done[i] = todo_done[i];
            free(dynamic_todos[i]);
        }
        free(dynamic_todos);
        free(todo_done);
    }

    dynamic_todos = new_array;
    todo_done = new_done;
    todo_capacity = new_capacity;
}

static inline void load(void) {
    char buffer[1024];
    for (int i = 0; i < 1024; i++) buffer[i] = 0;
    total_todos = 0;

    read_file("/.todo", (void*)buffer, 1024);
    
    int line_start = 0;
    for (int i = 0; i < 1024; i++) {
        if (buffer[i] == '\0') break; 

        if (buffer[i] == '\n') {
            buffer[i] = '\0';
            if (str_strlen(buffer + line_start) > 0) {
                if (total_todos >= todo_capacity) {
                    grow_capacity();
                }
                
                char* line = buffer + line_start;
                if (line[0] == '1' && line[1] == ' ') {
                    todo_done[total_todos] = true;
                    str_strcpy(dynamic_todos[total_todos], line + 2);
                } else if (line[0] == '0' && line[1] == ' ') {
                    todo_done[total_todos] = false;
                    str_strcpy(dynamic_todos[total_todos], line + 2);
                } else {
                    todo_done[total_todos] = false;
                    str_strcpy(dynamic_todos[total_todos], line);
                }
                
                total_todos++;
            }
            line_start = i + 1;
        }
    }
    selected_index = 0;
}

static inline void save(void) {
    char buffer_write[1024];
    for (int i = 0; i < 1024; i++) buffer_write[i] = 0;

    int write_ptr = 0;
    for (int i = 0; i < total_todos; i++) {
        int len = (int)str_strlen(dynamic_todos[i]);
        if (write_ptr + len + 4 >= 1024) break; 

        if (todo_done[i]) {
            buffer_write[write_ptr++] = '1';
        } else {
            buffer_write[write_ptr++] = '0';
        }
        buffer_write[write_ptr++] = ' ';

        str_strcpy(buffer_write + write_ptr, dynamic_todos[i]);
        write_ptr += len;
        buffer_write[write_ptr++] = '\n';
    }

    write_file("/.todo", buffer_write); 
}

static inline void render(void) {
    clear();
    
    print_at("=== To Do List ===", 2, 1, COLOR_BRIGHT_CYAN);
    
    if (total_todos == 0) {
        print_at(" (Your list is empty! Press 'A' to create a task) ", 4, 2, COLOR_BRIGHT_YELLOW);
    } else {
        for (int i = 0; i < total_todos; i++) {
            uint32_t text_color = (i == selected_index) ? COLOR_BRIGHT_WHITE : COLOR_GRAY;
            uint32_t y_pos = (uint32_t)(2 + i);
            
            if (i == selected_index) {
                if (todo_done[i]) {
                    print_at("->  [X] ", 2, y_pos, COLOR_BRIGHT_GREEN);
                } else {
                    print_at("->  [ ] ", 2, y_pos, COLOR_BRIGHT_GREEN);
                }
            } else {
                if (todo_done[i]) {
                    print_at("    [X] ", 2, y_pos, COLOR_GREEN);
                } else {
                    print_at("    [ ] ", 2, y_pos, COLOR_DARK_GRAY);
                }
            }
            
            print_at(dynamic_todos[i], 12, y_pos, text_color);
        }
    }
    
    print_at("--------------------------------------------------------------------------------", 2, 18, COLOR_DARK_GRAY);
    print_at("[A] Add Task   [ENTER] Toggle Done   [BACKSPACE] Delete Task   [Q] Exit App", 2, 20, COLOR_BRIGHT_ORANGE);
}

void _start(void) {
    set_text_mode();
    clear();

    grow_capacity();

    if (!file_exists("/.todo")) {
        create_file("/.todo");
    }

    load();

    while (1) {
        render();
        uint64_t key = get_key();

        if (key == 'q' || key == 'Q') {
            break;
        }

        if (key == KEY_UP) {
            if (selected_index > 0) {
                selected_index--;
            }
        }

        if (key == KEY_DOWN) {
            if (selected_index < total_todos - 1) {
                selected_index++;
            }
        }

        if (key == 'a' || key == 'A') {
            char new_task[TODO_LEN];
            for (int i = 0; i < TODO_LEN; i++) new_task[i] = 0;

            println("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
            input("  [new]> ", new_task, TODO_LEN - 1);

            if (str_strlen(new_task) > 0) {
                if (total_todos >= todo_capacity) {
                    grow_capacity();
                }
                str_strcpy(dynamic_todos[total_todos], new_task);
                todo_done[total_todos] = false;
                total_todos++;
                save();
            }
        }

        if (key == KEY_ENTER) {
            if (total_todos > 0) {
                todo_done[selected_index] = !todo_done[selected_index];
                save();
            }
        }

        if (key == KEY_BACKSPACE) {
            if (total_todos > 0) {
                char* removed_str = dynamic_todos[selected_index];
                bool removed_done = todo_done[selected_index];
                
                for (int i = selected_index; i < total_todos - 1; i++) {
                    dynamic_todos[i] = dynamic_todos[i + 1];
                    todo_done[i] = todo_done[i + 1];
                }
                
                dynamic_todos[total_todos - 1] = removed_str;
                todo_done[total_todos - 1] = removed_done;
                
                for (int j = 0; j < TODO_LEN; j++) dynamic_todos[total_todos - 1][j] = '\0';
                todo_done[total_todos - 1] = false;
                
                total_todos--;
                
                if (selected_index >= total_todos && selected_index > 0) {
                    selected_index = total_todos - 1;
                }
                save();
            }
        }
    }

    clear();
    sys_exit(0);
}
