#include <boot/kernel.h>
#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <string.h>
#include <bool.h>
#include <time.h>
#include <system.h>
#include <basicutil.h>

#define BASH_LINE_SIZE 256
#define BASH_TOKEN_COUNT 32
#define BASH_VARIABLE_COUNT 12
#define BASH_VARIABLE_NAME_SIZE 32
#define BASH_VARIABLE_VALUE_SIZE 128

typedef enum {
    TOKEN_WORD,
    TOKEN_SEMICOLON,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_UNSUPPORTED
} token_type_t;

typedef struct {
    token_type_t type;
    char *value;
} bash_token_t;

typedef struct {
    char name[BASH_VARIABLE_NAME_SIZE];
    char value[BASH_VARIABLE_VALUE_SIZE];
} bash_variable_t;

static bash_variable_t variables[BASH_VARIABLE_COUNT];
static size_t variable_count;
static int variables_initialized;
static int shell_status;
static bool shell_exit_requested;

extern uint32_t saved_color;

static size_t text_length(const char *text) {
    size_t length = 0;
    if (text != NULL) {
        while (text[length] != '\0') {
            length++;
        }
    }
    return length;
}

static int text_copy(char *destination, size_t capacity, const char *source) {
    size_t length = text_length(source);
    if (destination == NULL || capacity == 0 || length >= capacity) {
        return -1;
    }
    for (size_t i = 0; i <= length; i++) {
        destination[i] = source[i];
    }
    return 0;
}

static int append_text(char *destination, size_t capacity, size_t *length,
                       const char *text) {
    size_t append_length = text_length(text);
    if (*length > capacity || append_length >= capacity - *length) {
        return -1;
    }
    for (size_t i = 0; i < append_length; i++) {
        destination[*length + i] = text[i];
    }
    *length += append_length;
    destination[*length] = '\0';
    return 0;
}

static void initialize_variables(void) {
    if (variables_initialized) {
        return;
    }
    variables_initialized = 1;
    variables[0].name[0] = '\0';
}

static const char *get_variable(const char *name, size_t name_length) {
    static char status_text[16];
    static char home[128];
    initialize_variables();

    if (name_length == 1 && name[0] == '?') {
        str_itoa(shell_status, status_text);
        return status_text;
    }
    for (size_t i = 0; i < variable_count; i++) {
        if (text_length(variables[i].name) == name_length &&
            str_strncmp(variables[i].name, name, name_length) == 0) {
            return variables[i].value;
        }
    }
    if (name_length == 4 && str_strncmp(name, "USER", 4) == 0) {
        return current_user;
    }
    if (name_length == 4 && str_strncmp(name, "HOME", 4) == 0) {
        size_t home_length = 0;
        home[0] = '\0';
        if (append_text(home, sizeof(home), &home_length, "/users/") != 0 ||
            append_text(home, sizeof(home), &home_length, current_user) != 0) {
            return "";
        }
        return home;
    }
    if (name_length == 3 && str_strncmp(name, "PWD", 3) == 0) {
        const char *directory = fs_dirname();
        return directory != NULL ? directory : "";
    }

    return "";
}

static int valid_variable_name(const char *name, size_t length) {
    if (length == 0 || length >= BASH_VARIABLE_NAME_SIZE) {
        return 0;
    }
    if (!((name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z') || name[0] == '_')) {
        return 0;
    }
    for (size_t i = 1; i < length; i++) {
        if (!((name[i] >= 'A' && name[i] <= 'Z') ||
              (name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= '0' && name[i] <= '9') || name[i] == '_')) {
            return 0;
        }
    }
    return 1;
}

static int set_variable(const char *name, size_t name_length, const char *value) {
    initialize_variables();
    if (!valid_variable_name(name, name_length) ||
        text_length(value) >= BASH_VARIABLE_VALUE_SIZE) {
        return -1;
    }

    for (size_t i = 0; i < variable_count; i++) {
        if (text_length(variables[i].name) == name_length &&
            str_strncmp(variables[i].name, name, name_length) == 0) {
            if (text_copy(variables[i].value, sizeof(variables[i].value), value) != 0) {
                return -1;
            }
            return 0;
        }
    }
    if (variable_count == BASH_VARIABLE_COUNT) {
        return -1;
    }

    for (size_t i = 0; i < name_length; i++) {
        variables[variable_count].name[i] = name[i];
    }
    variables[variable_count].name[name_length] = '\0';
    if (text_copy(variables[variable_count].value,
                  sizeof(variables[variable_count].value), value) != 0) {
        return -1;
    }
    variable_count++;
    return 0;
}

static int append_expansion(const char *source, size_t *source_index,
                            char quote, char *output, size_t output_capacity,
                            size_t *output_length) {
    size_t start;
    size_t name_length;
    const char *value;

    if (source[*source_index] != '$') {
        return -1;
    }
    (*source_index)++;

    if (source[*source_index] == '?') {
        value = get_variable("?", 1);
        (*source_index)++;
        return append_text(output, output_capacity, output_length, value);
    }

    if (source[*source_index] == '{') {
        (*source_index)++;
        start = *source_index;
        while (source[*source_index] != '\0' && source[*source_index] != '}') {
            (*source_index)++;
        }
        if (source[*source_index] != '}') {
            return -1;
        }
        name_length = *source_index - start;
        (*source_index)++;
        if (!valid_variable_name(source + start, name_length)) {
            return -1;
        }
        value = get_variable(source + start, name_length);
        return append_text(output, output_capacity, output_length, value);
    }

    start = *source_index;
    if (!(((source[start] >= 'A' && source[start] <= 'Z') ||
           (source[start] >= 'a' && source[start] <= 'z') ||
           source[start] == '_'))) {
        return append_text(output, output_capacity, output_length, "$");
    }
    (*source_index)++;
    while ((source[*source_index] >= 'A' && source[*source_index] <= 'Z') ||
           (source[*source_index] >= 'a' && source[*source_index] <= 'z') ||
           (source[*source_index] >= '0' && source[*source_index] <= '9') ||
           source[*source_index] == '_') {
        (*source_index)++;
    }
    name_length = *source_index - start;
    value = get_variable(source + start, name_length);
    (void)quote;
    return append_text(output, output_capacity, output_length, value);
}

static int add_token(bash_token_t *tokens, size_t *token_count,
                     token_type_t type, char *value) {
    if (*token_count == BASH_TOKEN_COUNT) {
        return -1;
    }
    tokens[*token_count].type = type;
    tokens[*token_count].value = value;
    (*token_count)++;
    return 0;
}

static int tokenize(char *source, bash_token_t *tokens, size_t *token_count,
                    char *storage, size_t storage_capacity) {
    size_t read_index = 0;
    size_t write_index = 0;
    size_t word_start = 0;
    size_t word_length = 0;
    char quote = '\0';
    int in_word = 0;

    *token_count = 0;
    storage[0] = '\0';

    while (source[read_index] != '\0') {
        char character = source[read_index];

        if (quote == '\0' && (character == ' ' || character == '\t')) {
            if (in_word) {
                if (write_index >= storage_capacity) {
                    return -1;
                }
                storage[write_index++] = '\0';
                if (add_token(tokens, token_count, TOKEN_WORD,
                              storage + word_start) != 0) {
                    return -1;
                }
                in_word = 0;
                word_length = 0;
            }
            read_index++;
            continue;
        }

        if (quote == '\0' &&
            (character == ';' || character == '&' || character == '|' ||
             character == '<' || character == '>')) {
            if (in_word) {
                if (write_index >= storage_capacity) {
                    return -1;
                }
                storage[write_index++] = '\0';
                if (add_token(tokens, token_count, TOKEN_WORD,
                              storage + word_start) != 0) {
                    return -1;
                }
                in_word = 0;
                word_length = 0;
            }

            token_type_t type = TOKEN_UNSUPPORTED;
            if (character == ';') {
                type = TOKEN_SEMICOLON;
                read_index++;
            } else if ((character == '&' || character == '|') &&
                       source[read_index + 1] == character) {
                type = character == '&' ? TOKEN_AND : TOKEN_OR;
                read_index += 2;
            } else if ((character == '<' || character == '>') &&
                       source[read_index + 1] == character) {
                read_index += 2;
            } else {
                read_index++;
            }
            if (add_token(tokens, token_count, type, NULL) != 0) {
                return -1;
            }
            continue;
        }

        if (!in_word) {
            if (*token_count == BASH_TOKEN_COUNT || write_index >= storage_capacity) {
                return -1;
            }
            word_start = write_index;
            in_word = 1;
        }

        if (quote == '\0' && (character == '\'' || character == '"')) {
            quote = character;
            read_index++;
            continue;
        }
        if (quote != '\0' && character == quote) {
            quote = '\0';
            read_index++;
            continue;
        }
        if (character == '\\' && quote != '\'') {
            if (source[read_index + 1] == '\0') {
                return -1;
            }
            read_index++;
            character = source[read_index++];
        } else if (character == '$' && quote != '\'') {
            if (append_expansion(source, &read_index, quote, storage,
                                 storage_capacity, &write_index) != 0) {
                return -1;
            }
            word_length = write_index - word_start;
            continue;
        } else {
            read_index++;
        }

        if (write_index + 1 >= storage_capacity) {
            return -1;
        }
        storage[write_index++] = character;
        storage[write_index] = '\0';
        word_length++;
    }

    if (quote != '\0') {
        return -1;
    }
    if (in_word) {
        if (write_index >= storage_capacity) {
            return -1;
        }
        storage[write_index++] = '\0';
        if (add_token(tokens, token_count, TOKEN_WORD, storage + word_start) != 0) {
            return -1;
        }
    }
    (void)word_length;
    return 0;
}

static int canonicalize_path(const char *path, char *output, size_t capacity) {
    const char *working_directory = fs_dirname();
    char combined[512] = {0};
    size_t combined_length = 0;
    size_t output_length = 1;

    if (path == NULL || path[0] == '\0' || output == NULL || capacity < 2) {
        return -1;
    }
    combined[0] = '\0';

    if (path[0] != '/') {
        if (working_directory == NULL ||
            append_text(combined, sizeof(combined), &combined_length,
                        working_directory) != 0 ||
            (combined_length > 0 && combined[combined_length - 1] != '/' &&
             append_text(combined, sizeof(combined), &combined_length, "/") != 0) ||
            append_text(combined, sizeof(combined), &combined_length, path) != 0) {
            return -1;
        }
    } else if (text_copy(combined, sizeof(combined), path) != 0) {
        return -1;
    }

    output[0] = '/';
    output[1] = '\0';
    size_t index = 0;
    while (combined[index] != '\0') {
        while (combined[index] == '/') {
            index++;
        }
        if (combined[index] == '\0') {
            break;
        }

        size_t segment_start = index;
        while (combined[index] != '\0' && combined[index] != '/') {
            index++;
        }
        size_t segment_length = index - segment_start;
        if (segment_length == 1 && combined[segment_start] == '.') {
            continue;
        }
        if (segment_length == 2 && combined[segment_start] == '.' &&
            combined[segment_start + 1] == '.') {
            while (output_length > 1 && output[output_length - 1] != '/') {
                output_length--;
            }
            if (output_length > 1) {
                output_length--;
            }
            output[output_length] = '\0';
            continue;
        }

        size_t separator_length = output_length > 1 ? 1 : 0;
        if (output_length + separator_length + segment_length >= capacity) {
            return -1;
        }
        if (separator_length != 0) {
            output[output_length++] = '/';
        }
        for (size_t i = 0; i < segment_length; i++) {
            output[output_length++] = combined[segment_start + i];
        }
        output[output_length] = '\0';
    }
    return 0;
}

static int authorize_path(const char *path) {
    char canonical_path[256];
    if (canonicalize_path(path, canonical_path, sizeof(canonical_path)) != 0) {
        dogeio_text_println("bash: invalid or overlong path");
        return 0;
    }
    if (!system_can_access_path(current_user, canonical_path)) {
        dogeio_text_println("bash: permission denied");
        return 0;
    }
    return 1;
}

static int build_home_path(char *path, size_t capacity) {
    return text_copy(path, capacity, get_variable("HOME", 4));
}

static int build_history_path(char *path, size_t capacity) {
    size_t length = 0;
    path[0] = '\0';
    if (append_text(path, capacity, &length, "/users/") != 0 ||
        append_text(path, capacity, &length, current_user) != 0 ||
        append_text(path, capacity, &length, "/.history") != 0) {
        return -1;
    }
    return 0;
}

static int append_history(const char *command) {
    char history_path[160];
    char line[BASH_LINE_SIZE + 1];
    size_t length = text_length(command);
    if (length == 0) {
        return 0;
    }
    if (build_history_path(history_path, sizeof(history_path)) != 0 ||
        length + 1 >= sizeof(line)) {
        return -1;
    }
    if (!fs_exists(history_path) && fs_create(history_path) != 0) {
        return -1;
    }
    for (size_t i = 0; i < length; i++) {
        line[i] = command[i];
    }
    line[length++] = '\n';
    return fs_append_data(history_path, (const uint8_t *)line,
                          (uint32_t)length);
}

static int print_file(const char *path) {
    static char contents[8192];
    int bytes_read = fs_read((char *)path, contents, sizeof(contents) - 1);
    if (bytes_read < 0 || bytes_read >= (int)sizeof(contents)) {
        dogeio_text_println("bash: unable to read file");
        return 1;
    }
    contents[bytes_read] = '\0';
    dogeio_text_print(contents);
    if (bytes_read == 0 || contents[bytes_read - 1] != '\n') {
        dogeio_text_println("");
    }
    return 0;
}

static int show_history(void) {
    static char contents[8192];
    char history_path[160];
    if (build_history_path(history_path, sizeof(history_path)) != 0) {
        dogeio_text_println("bash: unable to build history path");
        return 1;
    }
    int bytes_read = fs_read(history_path, contents, sizeof(contents) - 1);
    if (bytes_read <= 0) {
        dogeio_text_println("bash: no history available");
        return bytes_read < 0 ? 1 : 0;
    }
    contents[bytes_read] = '\0';
    dogeio_text_print(contents);
    if (contents[bytes_read - 1] != '\n') {
        dogeio_text_println("");
    }
    return 0;
}

static int join_arguments(int argc, char **argv, int first,
                          char *output, size_t capacity) {
    size_t length = 0;
    output[0] = '\0';
    for (int i = first; i < argc; i++) {
        if (i != first &&
            append_text(output, capacity, &length, " ") != 0) {
            return -1;
        }
        if (append_text(output, capacity, &length, argv[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int print_help(void) {
    static const char *const help[] = {
        "Mini Bash for WindogeOS",
        "Built-ins: cd pwd ls echo cat touch mkdir rm mv cp clear history ver",
        "           export unset env date time whoami uname calc edit run",
        "           cpuinfo raminfo hexdump pci settings fetch true false exit",
        "Syntax: quotes and escapes, $NAME and ${NAME}, ;, &&, ||",
        "This shell does not implement processes, job control, pipes, or scripts."
    };
    for (size_t i = 0; i < sizeof(help) / sizeof(help[0]); i++) {
        dogeio_text_println(help[i]);
    }
    return 0;
}

static int execute_command(int argc, char **argv) {
    char path[256];
    if (argc == 0) {
        return 0;
    }

    size_t name_length = text_length(argv[0]);
    for (size_t i = 0; i < name_length; i++) {
        if (argv[0][i] == '=') {
            if (set_variable(argv[0], i, argv[0] + i + 1) != 0) {
                dogeio_text_println("bash: invalid or too many variables");
                return 1;
            }
            return 0;
        }
    }

    if (str_strcmp(argv[0], ":") == 0 || str_strcmp(argv[0], "true") == 0) {
        return 0;
    }
    if (str_strcmp(argv[0], "false") == 0) {
        return 1;
    }
    if (str_strcmp(argv[0], "exit") == 0) {
        int exit_status = shell_status;
        if (argc > 2) {
            dogeio_text_println("bash: exit: too many arguments");
            return 1;
        }
        if (argc == 2) {
            unsigned int parsed_status = 0;
            if (argv[1][0] == '\0') {
                dogeio_text_println("bash: exit: numeric argument required");
                return 2;
            }
            for (size_t i = 0; argv[1][i] != '\0'; i++) {
                if (argv[1][i] < '0' || argv[1][i] > '9' ||
                    parsed_status > 25U) {
                    dogeio_text_println("bash: exit: numeric argument required");
                    return 2;
                }
                parsed_status = parsed_status * 10U +
                                (unsigned int)(argv[1][i] - '0');
            }
            if (parsed_status > 255U) {
                dogeio_text_println("bash: exit: numeric argument required");
                return 2;
            }
            exit_status = (int)parsed_status;
        }
        shell_exit_requested = true;
        return exit_status;
    }
    if (str_strcmp(argv[0], "help") == 0) {
        return print_help();
    }
    if (str_strcmp(argv[0], "clear") == 0) {
        dogeio_text_clear();
        return 0;
    }
    if (str_strcmp(argv[0], "pwd") == 0) {
        const char *working_directory = fs_dirname();
        if (working_directory == NULL) {
            dogeio_text_println("bash: unable to get current directory");
            return 1;
        }
        dogeio_text_println(working_directory);
        return 0;
    }
    if (str_strcmp(argv[0], "whereami") == 0) {
        argv[0] = "pwd";
        return execute_command(argc, argv);
    }
    if (str_strcmp(argv[0], "cd") == 0) {
        char previous_directory[256];
        char *target;
        int print_directory = 0;
        if (argc > 2) {
            dogeio_text_println("bash: cd: too many arguments");
            return 1;
        }
        if (argc == 1 || str_strcmp(argv[1], "~") == 0) {
            if (build_home_path(path, sizeof(path)) != 0) {
                dogeio_text_println("bash: invalid home directory");
                return 1;
            }
            target = path;
        } else if (str_strcmp(argv[1], "-") == 0) {
            if (text_copy(path, sizeof(path), get_variable("OLDPWD", 6)) != 0) {
                dogeio_text_println("bash: cd: OLDPWD not set");
                return 1;
            }
            target = path;
            print_directory = 1;
        } else {
            target = argv[1];
        }
        if (!authorize_path(target)) {
            return 1;
        }
        const char *current_directory = fs_dirname();
        if (current_directory == NULL ||
            text_copy(previous_directory, sizeof(previous_directory),
                      current_directory) != 0) {
            dogeio_text_println("bash: cd: unable to determine current directory");
            return 1;
        }
        if (fs_chdir(target) != 0) {
            dogeio_text_print("bash: cd: ");
            dogeio_text_print(target);
            dogeio_text_println(": no such directory");
            return 1;
        }
        current_directory = fs_dirname();
        if (current_directory == NULL ||
            set_variable("OLDPWD", 6, previous_directory) != 0 ||
            set_variable("PWD", 3, current_directory) != 0) {
            dogeio_text_println("bash: cd: directory changed, but PWD could not be updated");
            return 1;
        }
        if (print_directory) {
            dogeio_text_println(current_directory);
        }
        return 0;
    }
    if (str_strcmp(argv[0], "ls") == 0 ||
        str_strcmp(argv[0], "dir") == 0) {
        int show_hidden = 0;
        const char *directory = NULL;
        for (int i = 1; i < argc; i++) {
            if (str_strcmp(argv[i], "-a") == 0 ||
                str_strcmp(argv[i], "-A") == 0 ||
                str_strcmp(argv[i], "--all") == 0 ||
                str_strcmp(argv[i], "--hidden") == 0 ||
                str_strcmp(argv[i], "-la") == 0 ||
                str_strcmp(argv[i], "-al") == 0) {
                show_hidden = 1;
            } else if (directory == NULL) {
                directory = argv[i];
            } else {
                dogeio_text_println("bash: ls: too many paths");
                return 1;
            }
        }
        if (directory == NULL) {
            return fs_list_dir(show_hidden) == 0 ? 0 : 1;
        }
        if (!authorize_path(directory)) {
            return 1;
        }
        return fs_list(directory, show_hidden) == 0 ? 0 : 1;
    }
    if (str_strcmp(argv[0], "echo") == 0 ||
        str_strcmp(argv[0], "print") == 0) {
        int first = 1;
        int newline = 1;
        char output[BASH_LINE_SIZE];
        if (argc > 1 && str_strcmp(argv[1], "-n") == 0) {
            newline = 0;
            first++;
        }
        if (join_arguments(argc, argv, first, output, sizeof(output)) != 0) {
            dogeio_text_println("bash: echo: argument list too long");
            return 1;
        }
        if (newline) {
            dogeio_text_println(output);
        } else {
            dogeio_text_print(output);
        }
        return 0;
    }
    if (str_strcmp(argv[0], "cat") == 0 ||
        str_strcmp(argv[0], "read") == 0) {
        if (argc < 2) {
            dogeio_text_println("bash: cat: missing file operand");
            return 1;
        }
        for (int i = 1; i < argc; i++) {
            if (!authorize_path(argv[i])) {
                return 1;
            }
            if (!fs_exists(argv[i]) || print_file(argv[i]) != 0) {
                dogeio_text_print("bash: cat: ");
                dogeio_text_print(argv[i]);
                dogeio_text_println(": unable to read");
                return 1;
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "touch") == 0 ||
        str_strcmp(argv[0], "create") == 0) {
        if (argc < 2) {
            dogeio_text_println("bash: touch: missing file operand");
            return 1;
        }
        for (int i = 1; i < argc; i++) {
            if (!authorize_path(argv[i])) {
                return 1;
            }
            if (!fs_exists(argv[i]) && fs_create(argv[i]) != 0) {
                dogeio_text_print("bash: touch: cannot create ");
                dogeio_text_println(argv[i]);
                return 1;
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "mkdir") == 0) {
        if (argc != 2) {
            dogeio_text_println("bash: mkdir: usage: mkdir <directory>");
            return 1;
        }
        if (!authorize_path(argv[1])) {
            return 1;
        }
        if (fs_mkdir(argv[1]) != 0) {
            dogeio_text_print("bash: mkdir: cannot create ");
            dogeio_text_println(argv[1]);
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "rm") == 0 ||
        str_strcmp(argv[0], "del") == 0 ||
        str_strcmp(argv[0], "delete") == 0) {
        if (argc < 2) {
            dogeio_text_println("bash: rm: missing file operand");
            return 1;
        }
        for (int i = 1; i < argc; i++) {
            if (!authorize_path(argv[i])) {
                return 1;
            }
            if (fs_delete(argv[i]) != 1) {
                dogeio_text_print("bash: rm: cannot remove ");
                dogeio_text_println(argv[i]);
                return 1;
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "mv") == 0 ||
        str_strcmp(argv[0], "rename") == 0 ||
        str_strcmp(argv[0], "cp") == 0) {
        if (argc != 3) {
            dogeio_text_print("bash: ");
            dogeio_text_print(argv[0]);
            dogeio_text_println(": usage: command <source> <destination>");
            return 1;
        }
        if (!authorize_path(argv[1]) || !authorize_path(argv[2])) {
            return 1;
        }
        int is_copy = str_strcmp(argv[0], "cp") == 0;
        int is_move = str_strcmp(argv[0], "mv") == 0;
        int result = is_copy ? fs_copy(argv[1], argv[2])
                             : (is_move ? fs_move(argv[1], argv[2])
                                        : fs_rename(argv[1], argv[2]));
        if ((is_copy || is_move) ? result != 0 : result != 1) {
            dogeio_text_print("bash: ");
            dogeio_text_print(argv[0]);
            dogeio_text_println(": operation failed");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "write") == 0 ||
        str_strcmp(argv[0], "sed") == 0) {
        char text[BASH_LINE_SIZE];
        if (argc != 2) {
            dogeio_text_println("bash: write: usage: write <file>");
            return 1;
        }
        if (!authorize_path(argv[1])) {
            return 1;
        }
        dogeio_text_input("text> ", text, sizeof(text));
        if (fs_write(argv[1], text) != 0) {
            dogeio_text_println("bash: unable to write file");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "date") == 0) {
        dogeio_text_println(date_get());
        return 0;
    }
    if (str_strcmp(argv[0], "time") == 0) {
        dogeio_text_println(time_get());
        return 0;
    }
    if (str_strcmp(argv[0], "whoami") == 0) {
        dogeio_text_println(current_user);
        return 0;
    }
    if (str_strcmp(argv[0], "ver") == 0) {
        dogeio_text_println("WindogeOS Mini Bash");
        return 0;
    }
    if (str_strcmp(argv[0], "hostname") == 0) {
        dogeio_text_println(computer_name[0] != '\0' ? computer_name : "windoge");
        return 0;
    }
    if (str_strcmp(argv[0], "uname") == 0) {
        dogeio_text_println("WindogeOS");
        return 0;
    }
    if (str_strcmp(argv[0], "cpuinfo") == 0) {
        dogeio_text_println(cpuid());
        return 0;
    }
    if (str_strcmp(argv[0], "raminfo") == 0) {
        char ram_text[32];
        uint64_to_str(get_ram(), ram_text);
        dogeio_text_print("RAM: ");
        dogeio_text_print(ram_text);
        dogeio_text_println(" bytes");
        return 0;
    }
    if (str_strcmp(argv[0], "hexdump") == 0) {
        if (argc != 2) {
            dogeio_text_println("bash: hexdump: usage: hexdump <file>");
            return 1;
        }
        if (!authorize_path(argv[1])) {
            return 1;
        }
        return util_hexdump(argv[1]) == 0 ? 0 : 1;
    }
    if (str_strcmp(argv[0], "pci") == 0) {
        dogeio_text_println("ADDR      DEVICE [VENDOR:DEVICE ID]");
        dogeio_text_println("------------------------------------");
        for (uint16_t bus = 0; bus < 256; bus++) {
            for (uint8_t slot = 0; slot < 32; slot++) {
                for (uint8_t function = 0; function < 8; function++) {
                    uint16_t vendor = pci_read_16((uint8_t)bus, slot,
                                                  function, 0x00);
                    if (vendor == 0xFFFF) {
                        if (function == 0) {
                            break;
                        }
                        continue;
                    }
                    uint16_t device = pci_read_16((uint8_t)bus, slot,
                                                  function, 0x02);
                    uint8_t class_code = pci_read_8((uint8_t)bus, slot,
                                                    function, 0x0B);
                    dogeio_print_hex8((uint8_t)bus);
                    dogeio_text_print(":");
                    dogeio_print_hex8(slot);
                    dogeio_text_print(".");
                    char function_text[2] = {(char)('0' + function), '\0'};
                    dogeio_text_print(function_text);
                    dogeio_text_print("    ");
                    dogeio_text_print(pci_class_to_name(class_code));
                    dogeio_text_print(" [");
                    dogeio_print_hex16(vendor);
                    dogeio_text_print(":");
                    dogeio_print_hex16(device);
                    dogeio_text_println("]");
                }
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "fetch") == 0) {
        system_fetch();
        return 0;
    }
    if (str_strcmp(argv[0], "settings") == 0) {
        system_settings();
        return 0;
    }
    if (str_strcmp(argv[0], "bash") == 0) {
        system_bash();
        shell_exit_requested = false;
        return 0;
    }
    if (str_strcmp(argv[0], "shutdown") == 0 ||
        str_strcmp(argv[0], "poweroff") == 0) {
        core_shutdown();
        return 0;
    }
    if (str_strcmp(argv[0], "reboot") == 0 ||
        str_strcmp(argv[0], "restart") == 0) {
        core_reboot();
        return 0;
    }
    if (str_strcmp(argv[0], "tab") == 0) {
        sys_switch_terminal();
        return 0;
    }
    if (str_strcmp(argv[0], "history") == 0) {
        return show_history();
    }
    if (str_strcmp(argv[0], "clear-history") == 0) {
        char history_path[160];
        if (build_history_path(history_path, sizeof(history_path)) != 0) {
            dogeio_text_println("bash: unable to build history path");
            return 1;
        }
        if (fs_exists(history_path) && fs_delete(history_path) != 1) {
            dogeio_text_println("bash: unable to clear history");
            return 1;
        }
        if (fs_create(history_path) != 0) {
            dogeio_text_println("bash: unable to recreate history");
            return 1;
        }
        return 0;
    }
    if (str_strcmp(argv[0], "export") == 0) {
        if (argc == 1) {
            for (size_t i = 0; i < variable_count; i++) {
                dogeio_text_print("declare -x ");
                dogeio_text_print(variables[i].name);
                dogeio_text_print("=\"");
                dogeio_text_print(variables[i].value);
                dogeio_text_println("\"");
            }
            return 0;
        }
        for (int i = 1; i < argc; i++) {
            size_t length = text_length(argv[i]);
            size_t equals = 0;
            while (equals < length && argv[i][equals] != '=') {
                equals++;
            }
            if (equals == length ||
                set_variable(argv[i], equals, argv[i] + equals + 1) != 0) {
                dogeio_text_println("bash: export: expected NAME=value");
                return 1;
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "unset") == 0) {
        if (argc < 2) {
            dogeio_text_println("bash: unset: missing variable name");
            return 1;
        }
        for (int argument = 1; argument < argc; argument++) {
            for (size_t i = 0; i < variable_count; i++) {
                if (str_strcmp(variables[i].name, argv[argument]) == 0) {
                    for (size_t j = i + 1; j < variable_count; j++) {
                        variables[j - 1] = variables[j];
                    }
                    variable_count--;
                    break;
                }
            }
        }
        return 0;
    }
    if (str_strcmp(argv[0], "env") == 0) {
        dogeio_text_print("USER=");
        dogeio_text_println(current_user);
        char home[128];
        if (build_home_path(home, sizeof(home)) == 0) {
            dogeio_text_print("HOME=");
            dogeio_text_println(home);
        }
        const char *working_directory = fs_dirname();
        dogeio_text_print("PWD=");
        dogeio_text_println(working_directory != NULL ? working_directory : "");
        for (size_t i = 0; i < variable_count; i++) {
            dogeio_text_print(variables[i].name);
            dogeio_text_print("=");
            dogeio_text_println(variables[i].value);
        }
        return 0;
    }
    if (str_strcmp(argv[0], "calc") == 0) {
        char expression[BASH_LINE_SIZE];
        char result_text[16];
        if (argc < 2 || join_arguments(argc, argv, 1, expression,
                                      sizeof(expression)) != 0) {
            dogeio_text_println("bash: calc: expected an expression");
            return 1;
        }
        str_itoa(util_calc(expression), result_text);
        dogeio_text_println(result_text);
        return 0;
    }
    if (str_strcmp(argv[0], "edit") == 0) {
        if (argc != 2) {
            dogeio_text_println("bash: edit: usage: edit <file>");
            return 1;
        }
        if (str_strcmp(argv[1], "--help") == 0) {
            dogeio_text_println("Dogeedit v2.1");
            dogeio_text_println("edit <file>");
            return 0;
        }
        if (str_strcmp(argv[1], "--version") == 0) {
            dogeio_text_println("Dogeedit v2.1");
            return 0;
        }
        if (!authorize_path(argv[1])) {
            return 1;
        }
        if (!fs_exists(argv[1]) && fs_create(argv[1]) != 0) {
            dogeio_text_println("bash: edit: unable to create file");
            return 1;
        }
        system_editor(argv[1]);
        return 0;
    }
    if (str_strcmp(argv[0], "run") == 0) {
        if (argc != 2) {
            dogeio_text_println("bash: run: usage: run <program>");
            return 1;
        }
        if (!authorize_path(argv[1])) {
            return 1;
        }
        if (!fs_exists(argv[1])) {
            dogeio_text_println("bash: run: file does not exist");
            return 1;
        }
        system_run_bin(argv[1], MAX_FLAT_BINARY_SIZE);
        return 0;
    }

    if (argc > 0 && str_strcmp(argv[0], ".") != 0 &&
        str_strcmp(argv[0], "..") != 0) {
        size_t command_length = text_length(argv[0]);
        int explicit_path = 0;
        for (size_t i = 0; argv[0][i] != '\0'; i++) {
            if (argv[0][i] == '/' || argv[0][i] == '\\') {
                explicit_path = 1;
                break;
            }
        }
        if (explicit_path) {
            if (command_length >= sizeof(path)) {
                dogeio_text_println("bash: path is too long");
                return 126;
            }
            if (!authorize_path(argv[0])) {
                return 126;
            }
            if (fs_exists(argv[0])) {
                system_run_bin_args(argv[0], MAX_FLAT_BINARY_SIZE,
                                    argc - 1, argv + 1);
                return 0;
            }
            dogeio_text_print("bash: ");
            dogeio_text_print(argv[0]);
            dogeio_text_println(": no such file or program");
            return 127;
        }
        if (command_length >= 124) {
            dogeio_text_print("bash: ");
            dogeio_text_print(argv[0]);
            dogeio_text_println(": command not found");
            return 127;
        }
        char app_name[128];
        size_t app_name_length = command_length;
        int has_extension = 0;
        if (app_name_length >= 4) {
            const char *extension = argv[0] + app_name_length - 4;
            has_extension = extension[0] == '.' &&
                            (extension[1] == 'b' || extension[1] == 'B') &&
                            (extension[2] == 'i' || extension[2] == 'I') &&
                            (extension[3] == 'n' || extension[3] == 'N');
        }
        if (has_extension) {
            if (text_copy(app_name, sizeof(app_name), argv[0]) != 0) {
                return 127;
            }
        } else {
            if (text_copy(app_name, sizeof(app_name), argv[0]) != 0 ||
                append_text(app_name, sizeof(app_name), &app_name_length,
                            ".bin") != 0) {
                return 127;
            }
        }
        for (size_t i = 0; app_name[i] != '\0'; i++) {
            if (app_name[i] == '/' || app_name[i] == '\\') {
                dogeio_text_println("bash: applications are run by name from /apps");
                return 126;
            }
        }
        if (text_copy(path, sizeof(path), "/apps/") != 0) {
            return 1;
        }
        size_t path_length = 6;
        if (append_text(path, sizeof(path), &path_length, app_name) != 0) {
            return 1;
        }
        if (!fs_exists(path)) {
            path[0] = '/';
            path[1] = '\0';
            path_length = 1;
            if (append_text(path, sizeof(path), &path_length, app_name) != 0) {
                return 1;
            }
        }
        if (fs_exists(path)) {
            system_run_bin_args(path, MAX_FLAT_BINARY_SIZE, argc - 1,
                                argv + 1);
            return 0;
        }
    }

    dogeio_text_print("bash: ");
    dogeio_text_print(argv[0]);
    dogeio_text_println(": command not found");
    return 127;
}

static int find_next_operator(const char *source, size_t start, size_t *end,
                              size_t *operator_length,
                              token_type_t *operator_type) {
    char quote = '\0';
    size_t index = start;

    while (source[index] != '\0') {
        char character = source[index];
        if (character == '\\' && quote != '\'') {
            if (source[index + 1] == '\0') {
                return -1;
            }
            index += 2;
            continue;
        }
        if (quote != '\0') {
            if (character == quote) {
                quote = '\0';
            }
            index++;
            continue;
        }
        if (character == '\'' || character == '"') {
            quote = character;
            index++;
            continue;
        }
        if (character == ';' || character == '&' || character == '|' ||
            character == '<' || character == '>') {
            *end = index;
            *operator_length = 1;
            *operator_type = TOKEN_UNSUPPORTED;
            if (character == ';') {
                *operator_type = TOKEN_SEMICOLON;
            } else if ((character == '&' || character == '|') &&
                       source[index + 1] == character) {
                *operator_type = character == '&' ? TOKEN_AND : TOKEN_OR;
                *operator_length = 2;
            } else if ((character == '<' || character == '>') &&
                       source[index + 1] == character) {
                *operator_length = 2;
            }
            return 0;
        }
        index++;
    }
    if (quote != '\0') {
        return -1;
    }
    *end = index;
    *operator_length = 0;
    *operator_type = TOKEN_WORD;
    return 0;
}

int system_bash_ex(char *command) {
    char source[BASH_LINE_SIZE];
    char segment[BASH_LINE_SIZE];
    char storage[BASH_LINE_SIZE];
    bash_token_t tokens[BASH_TOKEN_COUNT];
    size_t source_length;
    size_t source_index = 0;
    int status = 0;
    int should_execute = 1;
    int requires_command = 0;

    if (command == NULL || command[0] == '\0') {
        shell_exit_requested = false;
        return shell_status;
    }
    shell_exit_requested = false;
    if (text_copy(source, sizeof(source), command) != 0) {
        dogeio_text_println("bash: syntax error or command line too long");
        shell_status = 2;
        return shell_status;
    }
    if (append_history(source) != 0) {
        dogeio_text_println("bash: unable to save command history");
    }
    source_length = text_length(source);

    while (source_index < source_length && !shell_exit_requested) {
        size_t segment_start = source_index;
        size_t segment_end;
        size_t operator_length;
        size_t token_count = 0;
        token_type_t operator_type;
        int segment_has_command = 0;

        if (find_next_operator(source, segment_start, &segment_end,
                               &operator_length, &operator_type) != 0) {
            dogeio_text_println("bash: syntax error: unmatched quote or escape");
            status = 2;
            break;
        }

        size_t segment_length = segment_end - segment_start;
        for (size_t i = 0; i < segment_length; i++) {
            segment[i] = source[segment_start + i];
            if (source[segment_start + i] != ' ' &&
                source[segment_start + i] != '\t') {
                segment_has_command = 1;
            }
        }
        segment[segment_length] = '\0';

        if (should_execute && segment_has_command) {
            char *arguments[BASH_TOKEN_COUNT];
            int argument_count = 0;
            if (tokenize(segment, tokens, &token_count, storage,
                         sizeof(storage)) != 0) {
                dogeio_text_println("bash: syntax error or command line too long");
                status = 2;
                break;
            }
            for (size_t i = 0; i < token_count; i++) {
                arguments[argument_count++] = tokens[i].value;
            }
            if (argument_count != 0) {
                status = execute_command(argument_count, arguments);
                shell_status = status;
            }
        } else if (operator_type != TOKEN_SEMICOLON &&
                   operator_type != TOKEN_WORD && should_execute) {
            dogeio_text_println("bash: pipelines and redirections are not supported");
            status = 2;
            break;
        }

        if (requires_command && !segment_has_command) {
            dogeio_text_println("bash: syntax error near unexpected token");
            status = 2;
            break;
        }
        if (segment_has_command) {
            requires_command = 0;
        }
        if (operator_length == 0) {
            break;
        }
        source_index = segment_end + operator_length;

        if (operator_type == TOKEN_SEMICOLON) {
            should_execute = 1;
        } else if (operator_type == TOKEN_AND) {
            should_execute = status == 0;
            requires_command = 1;
        } else if (operator_type == TOKEN_OR) {
            should_execute = status != 0;
            requires_command = 1;
        } else {
            dogeio_text_println("bash: pipelines and redirections are not supported");
            status = 2;
            break;
        }

        size_t next = source_index;
        while (source[next] == ' ' || source[next] == '\t') {
            next++;
        }
        if (next == source_length) {
            if (operator_type == TOKEN_SEMICOLON) {
                break;
            }
            dogeio_text_println("bash: syntax error near unexpected token");
            status = 2;
            break;
        }
        source_index = next;
    }
    shell_status = status;
    return status;
}

static void print_prompt(void) {
    char home[128];
    const char *working_directory = fs_dirname();
    const char *display_directory = working_directory != NULL
                                        ? working_directory
                                        : "/";
    size_t home_length = 0;

    dogeio_text_color_change(0xFF55FF55);
    dogeio_text_print(current_user);
    dogeio_text_print("@");
    dogeio_text_print(computer_name[0] != '\0' ? computer_name : "windoge");
    dogeio_text_color_change(saved_color);
    dogeio_text_print(":");

    if (build_home_path(home, sizeof(home)) == 0) {
        home_length = text_length(home);
        if (str_strncmp(display_directory, home, home_length) == 0 &&
            (display_directory[home_length] == '\0' ||
             display_directory[home_length] == '/')) {
            dogeio_text_print("~");
            display_directory += home_length;
        }
    }
    dogeio_text_color_change(0xFF55AAFF);
    dogeio_text_print(display_directory);
    dogeio_text_color_change(saved_color);
    dogeio_text_print("$ ");
}

void system_bash(void) {
    char input[BASH_LINE_SIZE];
    shell_exit_requested = false;
    initialize_variables();

    while (!shell_exit_requested) {
        print_prompt();
        dogeio_text_input("", input, sizeof(input));
        if (input[0] == '\0') {
            continue;
        }
        system_bash_ex(input);
    }
}
