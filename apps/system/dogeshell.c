#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/string.h"
#include "../library/stddef.h"
#include "../library/stdint.h"
#include "../library/bool.h"

char user[64];
char* settings_path = "C:/.windoge";

static int setup(void) {
    str_strcpy(user, (char*)get_user());
}

static int append(char *destination, size_t capacity, size_t *length,
                            const char *source) {
    size_t source_length = str_strlen(source);
    if (*length >= capacity || source_length >= capacity - *length) {
        return -1;
    }
    for (size_t i = 0; i < source_length; i++) {
        destination[*length + i] = source[i];
    }
    *length += source_length;
    destination[*length] = '\0';
    return 0;
}

static int find_app(const char *command, char *app_path,
                              size_t app_path_capacity) {
    char filename[128];
    char path[160];
    size_t name_length = str_strlen(command);
    size_t path_length = 0;
    int has_extension = 0;

    if (name_length == 0 || name_length >= sizeof(filename)) {
        return 1;
    }
    for (size_t i = 0; i < name_length; i++) {
        if (command[i] == '/' || command[i] == '\\') {
            return 1;
        }
    }
    if (name_length >= 4) {
        const char *extension = command + name_length - 4;
        has_extension = extension[0] == '.' &&
                        (extension[1] == 'e' || extension[1] == 'E') &&
                        (extension[2] == 'l' || extension[2] == 'L') &&
                        (extension[3] == 'f' || extension[3] == 'F');
    }
    if (str_strncpy(filename, command, sizeof(filename)) != 0) {
        return 1;
    }
    if (!has_extension &&
        append(filename, sizeof(filename), &name_length, ".elf") != 0) {
        return 1;
    }
    if (append(path, sizeof(path), &path_length, "/apps/") != 0 ||
        append(path, sizeof(path), &path_length, filename) != 0) {
        return 1;
    }
    if (!file_exists(path)) {
        path[0] = '/';
        path[1] = '\0';
        path_length = 1;
        if (append(path, sizeof(path), &path_length, filename) != 0 ||
            !file_exists(path)) {
            return 1;
        }
    }
    if (str_strncpy(app_path, path, app_path_capacity) != 0) {
        return 1;
    }
    return 0;
}

int tokensize(const char *line, char *storage, size_t storage_size, char **arguments, size_t argument_capacity) {
    size_t read_index = 0;
    size_t write_index = 0;
    size_t argument_count = 0;

    while (line[read_index] != '\0') {
        char quote = '\0';
        size_t argument_start;
        while (line[read_index] == ' ' || line[read_index] == '\t') {
            read_index++;
        }
        if (line[read_index] == '\0') {
            break;
        }
        if (argument_count == argument_capacity || write_index >= storage_size) {
            return -1;
        }

        argument_start = write_index;
        arguments[argument_count++] = storage + argument_start;

        while (line[read_index] != '\0') {
            char character = line[read_index];
            if (quote == '\0' && (character == ' ' || character == '\t')) {
                break;
            }
            if (character == '\\' && quote != '\'') {
                read_index++;
                if (line[read_index] == '\0') {
                    return -1;
                }
                character = line[read_index++];
            } else if (quote != '\0') {
                if (character == quote) {
                    quote = '\0';
                    read_index++;
                    continue;
                }
                read_index++;
            } else if (character == '\'' || character == '"') {
                quote = character;
                read_index++;
                continue;
            } else {
                read_index++;
            }

            if (write_index + 1 >= storage_size) {
                return -1;
            }
            storage[write_index++] = character;
        }
        if (quote != '\0' || write_index >= storage_size) {
            return -1;
        }
        storage[write_index++] = '\0';
    }

    return (int)argument_count;
}

static int dogeshell_build_home(char *path, size_t capacity) {
    size_t length = 0;
    path[0] = '\0';
    return append(path, capacity, &length, "/users/") == 0 &&
                   append(path, capacity, &length, user) == 0
               ? 0
               : -1;
}

static int join_arguments(int argc, char **argv, int first,
                                    char *output, size_t capacity) {
    size_t length = 0;
    output[0] = '\0';
    for (int i = first; i < argc; i++) {
        if ((i != first &&
            append(output, capacity, &length, " ") != 0) ||
            append(output, capacity, &length, argv[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int execute(int argc, char** argv) {
    if (str_strcmp(argv[0], "print") == 0) {
        char output[512];
        if (join_arguments(argc, argv, 1, output, sizeof(output)) != 0) {
            println("Error: text is too long.");
            return 1;
        }
        println(output);
        return 0;
    }

    if (argc == 1) {
        char app_path[160];
        if (find_app(argv[0], app_path, sizeof(app_path)) == 0) {
            process_stack(app_path, 1, NULL);
            return 0;
        }
    } else {
        char app_path[160];
        if (find_app(argv[0], app_path, sizeof(app_path)) == 0) {
            process_stack(app_path, argc - 1, argv + 1);
            return 0;
        }
    }

    print(argv[0]);
    println(": command not found :(");
    return 127;
}