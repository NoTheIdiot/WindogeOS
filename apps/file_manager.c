#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/string.h"
#include "../library/stddef.h"

#define FILE_MANAGER_ENTRY_LIMIT 32
#define FILE_MANAGER_INPUT_SIZE 256

static char entries[FILE_MANAGER_ENTRY_LIMIT][256];

static void clear_buffer(char *buffer, size_t capacity) {
    for (size_t i = 0; i < capacity; i++) {
        buffer[i] = '\0';
    }
}

static void print_help(void) {
    println("Commands:");
    println("  ls                     list the current directory");
    println("  pwd                    show the current directory");
    println("  cd <directory>         change directory");
    println("  up                     move to the parent directory");
    println("  mkdir <directory>      create a directory");
    println("  touch <file>           create an empty file");
    println("  delete <path>          delete a file or empty directory");
    println("  rename <path> <path>   rename a file or directory");
    println("  copy <file> <path>     copy a file");
    println("  move <path> <path>     move a file or directory");
    println("  edit <file>            open a file in the text editor");
    println("  help                   show this help");
    println("  quit                   exit the file manager");
    println("Paths containing spaces are not supported.");
}

static void print_listing(void) {
    int64_t count = (int64_t)list_dir(entries, FILE_MANAGER_ENTRY_LIMIT);
    if (count < 0) {
        println("file_manager: unable to list this directory");
        return;
    }
    if (count == 0) {
        println("(empty directory)");
        return;
    }

    for (int64_t i = 0; i < count; i++) {
        dogec_stat_t entry_stat;
        entry_stat.size = 0;
        entry_stat.is_dir = 0;
        entry_stat.exists = 0;
        if ((int64_t)stat(entries[i], &entry_stat) < 0) {
            print("[?]   ");
            println(entries[i]);
        } else if (entry_stat.is_dir != 0) {
            print("[DIR] ");
            println(entries[i]);
        } else {
            char size_text[21];
            str_u64toa(entry_stat.size, size_text);
            print("[FILE ");
            print(size_text);
            print("] ");
            println(entries[i]);
        }
    }
}

static int split_command(char *line, char *tokens[], int token_capacity) {
    int count = 0;
    char *cursor = line;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        if (count == token_capacity) {
            return -1;
        }
        tokens[count++] = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return count;
}

static void report_path_operation(const char *operation, const char *path,
                                  uint64_t result) {
    if ((int64_t)result < 0) {
        print("file_manager: ");
        print(operation);
        print(" failed for ");
        println(path);
    } else {
        print(operation);
        print(": ");
        println(path);
    }
}

static int confirm_delete(const char *path) {
    char answer[8];
    clear_buffer(answer, sizeof(answer));
    print("Delete ");
    print(path);
    print("? [y/N] ");
    if ((int64_t)input("", answer, sizeof(answer)) < 0) {
        println("file_manager: input failed");
        return 0;
    }
    return answer[0] == 'y' || answer[0] == 'Y';
}

static int run_command(int count, char *tokens[]) {
    if (count == 0) {
        return 0;
    }
    if (str_strcmp(tokens[0], "quit") == 0 ||
        str_strcmp(tokens[0], "exit") == 0) {
        return 1;
    }
    if (str_strcmp(tokens[0], "help") == 0) {
        print_help();
    } else if (str_strcmp(tokens[0], "ls") == 0 && count == 1) {
        print_listing();
    } else if (str_strcmp(tokens[0], "pwd") == 0 && count == 1) {
        char cwd[FILE_MANAGER_INPUT_SIZE];
        clear_buffer(cwd, sizeof(cwd));
        if ((int64_t)get_cwd(cwd, sizeof(cwd)) < 0) {
            println("file_manager: unable to get the current directory");
        } else {
            println(cwd);
        }
    } else if (str_strcmp(tokens[0], "cd") == 0 && count == 2) {
        uint64_t result = change_dir(tokens[1]);
        report_path_operation("cd", tokens[1], result);
    } else if (str_strcmp(tokens[0], "up") == 0 && count == 1) {
        uint64_t result = change_dir("..");
        report_path_operation("cd", "..", result);
    } else if (str_strcmp(tokens[0], "mkdir") == 0 && count == 2) {
        uint64_t result = create_dir(tokens[1]);
        report_path_operation("mkdir", tokens[1], result);
    } else if (str_strcmp(tokens[0], "touch") == 0 && count == 2) {
        uint64_t result = create_file(tokens[1]);
        report_path_operation("touch", tokens[1], result);
    } else if (str_strcmp(tokens[0], "delete") == 0 && count == 2) {
        if (confirm_delete(tokens[1])) {
            report_path_operation("delete", tokens[1],
                                  delete_file(tokens[1]));
        } else {
            println("Delete cancelled.");
        }
    } else if (str_strcmp(tokens[0], "rename") == 0 && count == 3) {
        report_path_operation("rename", tokens[1],
                              rename_file(tokens[1], tokens[2]));
    } else if (str_strcmp(tokens[0], "copy") == 0 && count == 3) {
        report_path_operation("copy", tokens[1],
                              copy_file(tokens[1], tokens[2]));
    } else if (str_strcmp(tokens[0], "move") == 0 && count == 3) {
        report_path_operation("move", tokens[1],
                              move_file(tokens[1], tokens[2]));
    } else if (str_strcmp(tokens[0], "edit") == 0 && count == 2) {
        char *editor_args[] = {tokens[1]};
        if ((int64_t)exec_args("/apps/editor.bin", 1, editor_args) < 0) {
            println("file_manager: unable to launch the editor");
        }
    } else {
        println("Usage error; enter 'help' for commands.");
    }
    return 0;
}

void _start(int argc, char **argv) {
    if (argc > 1 || (argc == 1 &&
        (argv == NULL || argv[0] == NULL || argv[0][0] == '\0'))) {
        println("Usage: file_manager [directory]");
        sys_exit(2);
        return;
    }
    if (argc == 1 && (int64_t)change_dir(argv[0]) < 0) {
        println("file_manager: unable to open the requested directory");
        sys_exit(1);
    }

    println("WindogeOS file manager. Enter 'help' for commands.");
    print_listing();
    while (1) {
        char cwd[FILE_MANAGER_INPUT_SIZE];
        clear_buffer(cwd, sizeof(cwd));
        if ((int64_t)get_cwd(cwd, sizeof(cwd)) < 0) {
            println("file_manager: unable to get the current directory");
            sys_exit(1);
        }
        char command[FILE_MANAGER_INPUT_SIZE];
        clear_buffer(command, sizeof(command));
        print(cwd);
        if ((int64_t)input("> ", command, sizeof(command)) < 0) {
            println("file_manager: input failed");
            sys_exit(1);
        }
        char *tokens[4];
        int count = split_command(command, tokens, 4);
        if (count < 0) {
            println("file_manager: too many command arguments");
        } else if (run_command(count, tokens)) {
            sys_exit(0);
        }
    }
}
