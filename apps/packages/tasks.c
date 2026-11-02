#include "../library/dogeio.h"

#define TASK_CAPACITY (SCHEDULER_TASK_LIMIT - 1)
#define FIRST_TASK_ROW 6

static void print_number(uint32_t value, uint32_t x, uint32_t y,
                         uint32_t color) {
    char digits[11];
    uint32_t length = 0;
    do {
        digits[length++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0 && length < sizeof(digits));

    char text[11];
    for (uint32_t i = 0; i < length; i++) {
        text[i] = digits[length - i - 1];
    }
    text[length] = '\0';
    print_at(text, x, y, color);
}

static void render(const scheduler_task_info_t *tasks, uint64_t count,
                   int selected, uint32_t current_pid,
                   const char *status, uint32_t status_color) {
    clear();
    print_at("=== TASKS: PROCESS MONITOR ===", 2, 1, COLOR_BRIGHT_CYAN);
    print_at("Active processes:", 2, 2, COLOR_GRAY);
    print_number((uint32_t)count, 20, 2, COLOR_BRIGHT_WHITE);
    print_at("Current PID:", 27, 2, COLOR_GRAY);
    print_number(current_pid, 40, 2, COLOR_BRIGHT_WHITE);
    print_at("--------------------------------------------------------------------", 2, 3,
             COLOR_DARK_GRAY);
    print_at("PID", 5, 4, COLOR_BRIGHT_YELLOW);
    print_at("STATE", 17, 4, COLOR_BRIGHT_YELLOW);
    print_at("WORKSPACE", 31, 4, COLOR_BRIGHT_YELLOW);
    print_at("--------------------------------------------------------------------", 2, 5,
             COLOR_DARK_GRAY);

    if (count == 0) {
        print_at("No user processes are running.", 5, FIRST_TASK_ROW,
                 COLOR_BRIGHT_BLACK);
    } else {
        for (uint64_t i = 0; i < count; i++) {
            uint32_t y = FIRST_TASK_ROW + (uint32_t)i;
            bool is_selected = (int)i == selected;
            bool is_current = tasks[i].pid == current_pid;
            uint32_t color = is_selected ? COLOR_BRIGHT_WHITE : COLOR_GRAY;

            print_at(is_selected ? "> " : "  ", 2, y,
                     is_selected ? COLOR_BRIGHT_GREEN : COLOR_BACKGROUND);
            print_number(tasks[i].pid, 5, y, color);
            print_at(is_current ? "THIS APP" :
                     tasks[i].state == 2 ? "RUNNING" : "READY",
                     17, y, is_current ? COLOR_BRIGHT_CYAN : color);
            print_number(tasks[i].workspace_id + 1U, 31, y, color);
        }
    }

    print_at(status, 2, 16, status_color);
    print_at("--------------------------------------------------------------------", 2, 18,
             COLOR_DARK_GRAY);
    print_at("S: spawn ELF   Up/Down: select   K: stop   R: refresh   Q: exit",
             2, 20,
             COLOR_BRIGHT_ORANGE);
    print_at("Ctrl+Tab switches workspaces. Spawned apps run concurrently.",
             2, 21, COLOR_DARK_GRAY);
}

static int launch_process(void) {
    char path[128];
    char workspace_text[4];
    for (uint32_t i = 0; i < sizeof(path); i++) {
        path[i] = '\0';
    }
    for (uint32_t i = 0; i < sizeof(workspace_text); i++) {
        workspace_text[i] = '\0';
    }

    clear();
    print_at("=== SPAWN PROCESS ===", 2, 1, COLOR_BRIGHT_CYAN);
    print_at("Use an absolute path, e.g. /apps/myapp.elf", 2, 3,
             COLOR_GRAY);
    if (input("Executable: ", path, sizeof(path)) == (uint64_t)-1 ||
        path[0] == '\0') {
        return 0;
    }

    if (input("Workspace (1-4): ", workspace_text,
              sizeof(workspace_text)) == (uint64_t)-1 ||
        workspace_text[0] < '1' || workspace_text[0] > '4' ||
        workspace_text[1] != '\0') {
        return -1;
    }

    uint32_t workspace_id = (uint32_t)(workspace_text[0] - '1');
    uint64_t pid = spawn_task(path, workspace_id);
    if (pid == (uint64_t)-1) {
        return -1;
    }
    return 1;
}

void _start(void) {
    scheduler_task_info_t tasks[TASK_CAPACITY];
    int selected = 0;
    const char *status = "Ready.";
    uint32_t status_color = COLOR_GRAY;
    uint32_t current_pid = (uint32_t)syscall_0(GET_PID);

    set_text_mode();
    clear();

    while (1) {
        for (uint64_t i = 0; i < TASK_CAPACITY; i++) {
            tasks[i].pid = 0;
            tasks[i].state = 0;
            tasks[i].workspace_id = 0;
        }
        uint64_t count = list_tasks(tasks, TASK_CAPACITY);
        if (count == (uint64_t)-1) {
            clear();
            print_at("Unable to read the process list.", 2, 3,
                     COLOR_BRIGHT_RED);
            print_at("Press R to retry or Q to exit.", 2, 5, COLOR_WHITE);
            uint64_t key = get_key();
            if (key == 'q' || key == 'Q') {
                break;
            }
            continue;
        }

        if (count > TASK_CAPACITY) {
            count = TASK_CAPACITY;
        }
        if (count == 0) {
            selected = 0;
        } else if (selected >= (int)count) {
            selected = (int)count - 1;
        }

        render(tasks, count, selected, current_pid, status, status_color);
        uint64_t key = get_key();

        if (key == 'q' || key == 'Q') {
            break;
        }
        if (key == 's' || key == 'S') {
            int spawn_result = launch_process();
            if (spawn_result > 0) {
                status = "Process launched in the selected workspace.";
                status_color = COLOR_BRIGHT_GREEN;
            } else if (spawn_result < 0) {
                status = "Spawn failed: check path, access, workspace, or task limit.";
                status_color = COLOR_BRIGHT_RED;
            }
        } else if (key == 'r' || key == 'R') {
            status = "Process list refreshed.";
            status_color = COLOR_BRIGHT_GREEN;
        } else if (key == KEY_UP && selected > 0) {
            selected--;
            status = "Ready.";
            status_color = COLOR_GRAY;
        } else if (key == KEY_DOWN && count > 0 &&
                   selected < (int)count - 1) {
            selected++;
            status = "Ready.";
            status_color = COLOR_GRAY;
        } else if ((key == 'k' || key == 'K') && count > 0) {
            if (tasks[selected].pid == current_pid) {
                status = "Cannot stop the monitor itself.";
                status_color = COLOR_BRIGHT_RED;
            } else if (kill_task(tasks[selected].pid) != 0) {
                status = "Unable to stop the selected process.";
                status_color = COLOR_BRIGHT_RED;
            } else {
                status = "Process stopped.";
                status_color = COLOR_BRIGHT_GREEN;
            }
        }
    }

    clear();
    sys_exit(0);
}
