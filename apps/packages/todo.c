#include "../library/dogeio.h"
#include "../library/bool.h"
#include "../library/string.h"
#include "../library/dogelib.h"

#define TODO_LEN 240
#define MAX_TODOS 128
#define TODO_FILE_SIZE 32768
#define TODO_VISIBLE_ROWS 16

char** dynamic_todos = NULL;
bool* todo_done = NULL;
int total_todos = 0;
int todo_capacity = 0;
int selected_index = 0;
static int first_visible_index = 0;
static int load_truncated = 0;
static char todo_buffer[TODO_FILE_SIZE];

static int grow_capacity(void) {
    if (todo_capacity >= MAX_TODOS) return 0;

    int new_capacity = (todo_capacity == 0) ? 4 : todo_capacity * 2;
    if (new_capacity > MAX_TODOS) new_capacity = MAX_TODOS;
    if (dynamic_todos != NULL &&
        (total_todos < 0 || total_todos > todo_capacity ||
         total_todos > new_capacity)) {
        return 0;
    }

    char** new_array = (char**)malloc(sizeof(char*) * (size_t)new_capacity);
    bool* new_done = (bool*)malloc(sizeof(bool) * (size_t)new_capacity);
    if (new_array == NULL || new_done == NULL) {
        free(new_array);
        free(new_done);
        return 0;
    }

    memset(new_array, 0, sizeof(char*) * (size_t)new_capacity);
    
    for (int i = 0; i < new_capacity; i++) {
        new_array[i] = (char*)malloc(sizeof(char) * TODO_LEN);
        if (new_array[i] == NULL) {
            for (int j = 0; j < i; j++) free(new_array[j]);
            free(new_array);
            free(new_done);
            return 0;
        }
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
    return 1;
}

static inline int load(void) {
    for (int i = 0; i < TODO_FILE_SIZE; i++) todo_buffer[i] = 0;
    total_todos = 0;
    load_truncated = 0;

    int bytes = (int)read_file("/.todo", todo_buffer,
                               sizeof(todo_buffer) - 1);
    if (bytes < 0) return 0;
    todo_buffer[bytes] = '\0';
    if (bytes == (int)sizeof(todo_buffer) - 1) load_truncated = 1;

    int line_start = 0;
    for (int i = 0; i <= bytes; i++) {
        if (todo_buffer[i] == '\n' || todo_buffer[i] == '\0') {
            todo_buffer[i] = '\0';
            if (i > line_start) {
                if (total_todos >= todo_capacity) {
                    if (!grow_capacity()) return 0;
                }
                
                char* line = todo_buffer + line_start;
                int text_start = 0;
                if (line[0] == '1' && line[1] == ' ') {
                    todo_done[total_todos] = true;
                    text_start = 2;
                } else if (line[0] == '0' && line[1] == ' ') {
                    todo_done[total_todos] = false;
                    text_start = 2;
                } else {
                    todo_done[total_todos] = false;
                }
                int text_length = 0;
                while (line[text_start + text_length] != '\0' &&
                       text_length < TODO_LEN - 1) {
                    dynamic_todos[total_todos][text_length] =
                        line[text_start + text_length];
                    text_length++;
                }
                if (line[text_start + text_length] != '\0') {
                    load_truncated = 1;
                }
                dynamic_todos[total_todos][text_length] = '\0';
                total_todos++;
                if (total_todos == MAX_TODOS && i + 1 < bytes) {
                    load_truncated = 1;
                    break;
                }
            }
            line_start = i + 1;
        }
    }
    selected_index = 0;
    first_visible_index = 0;
    return 1;
}

static inline int save(void) {
    if (load_truncated) return 0;
    for (int i = 0; i < TODO_FILE_SIZE; i++) todo_buffer[i] = 0;

    int write_ptr = 0;
    for (int i = 0; i < total_todos; i++) {
        int len = (int)str_strlen(dynamic_todos[i]);
        if (write_ptr + len + 3 >= TODO_FILE_SIZE) return 0;

        if (todo_done[i]) {
            todo_buffer[write_ptr++] = '1';
        } else {
            todo_buffer[write_ptr++] = '0';
        }
        todo_buffer[write_ptr++] = ' ';

        str_strcpy(todo_buffer + write_ptr, dynamic_todos[i]);
        write_ptr += len;
        todo_buffer[write_ptr++] = '\n';
    }

    return (int64_t)write_file_raw("/.todo", (const uint8_t*)todo_buffer,
                                   (uint64_t)write_ptr) >= 0;
}

static inline void render(void) {
    clear();

    if (selected_index < first_visible_index) {
        first_visible_index = selected_index;
    } else if (selected_index >= first_visible_index + TODO_VISIBLE_ROWS) {
        first_visible_index = selected_index - TODO_VISIBLE_ROWS + 1;
    }
    
    print_at("=== To Do List ===", 2, 1, COLOR_BRIGHT_CYAN);
    
    if (total_todos == 0) {
        print_at(" (Your list is empty! Press 'A' to create a task) ", 4, 2, COLOR_BRIGHT_YELLOW);
    } else {
        for (int i = first_visible_index;
             i < total_todos && i < first_visible_index + TODO_VISIBLE_ROWS;
             i++) {
            uint32_t text_color = (i == selected_index) ? COLOR_BRIGHT_WHITE : COLOR_GRAY;
            uint32_t y_pos = (uint32_t)(2 + i - first_visible_index);
            
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
    
    if (load_truncated) {
        print_at("[List exceeds app limits; changes cannot be saved]", 2, 19,
                 COLOR_BRIGHT_RED);
    }
    print_at("--------------------------------------------------------------------------------", 2, 18, COLOR_DARK_GRAY);
    print_at("[A] Add Task   [ENTER] Toggle Done   [BACKSPACE] Delete Task   [Q] Exit App", 2, 20, COLOR_BRIGHT_ORANGE);
}

void _start(void) {
    set_text_mode();
    clear();

    if (!grow_capacity()) {
        println("Unable to allocate task list.");
        exit(1);
    }

    if (!file_exists("/.todo")) {
        create_file("/.todo");
    }

    if (!load()) {
        println("Unable to load task list.");
        exit(1);
    }

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
                if (total_todos >= MAX_TODOS) {
                    println("Task limit reached.");
                    continue;
                }
                if (total_todos >= todo_capacity && !grow_capacity()) {
                    println("Unable to allocate another task.");
                    continue;
                }
                str_strcpy(dynamic_todos[total_todos], new_task);
                todo_done[total_todos] = false;
                total_todos++;
                if (!save()) println("Unable to save task list.");
            }
        }

        if (key == KEY_ENTER) {
            if (total_todos > 0) {
                todo_done[selected_index] = !todo_done[selected_index];
                if (!save()) println("Unable to save task list.");
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
                if (!save()) println("Unable to save task list.");
            }
        }
    }

    clear();
    exit(0);
}
