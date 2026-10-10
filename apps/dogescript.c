#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/stdint.h"
#include "../library/stddef.h"
#include "../library/string.h"

#define DOGESCRIPT_MAX_SCRIPT_SIZE 65535
#define DOGESCRIPT_MAX_LINE_SIZE 256
#define DOGESCRIPT_READ_BUFFER_SIZE 512
#define DOGESCRIPT_MAX_TOKENS 16
#define DOGESCRIPT_MAX_SHELL_COMMAND_SIZE 4096
#define DOGESCRIPT_MAX_VARIABLES 32
#define DOGESCRIPT_VARIABLE_NAME_SIZE 32
#define DOGESCRIPT_VARIABLE_VALUE_SIZE 256
#define DOGESCRIPT_MAX_LOOP_DEPTH 32
#define DOGESCRIPT_MAX_STEPS 500000
#define DOGESCRIPT_MAX_REPEAT 100000

typedef struct {
    char name[DOGESCRIPT_VARIABLE_NAME_SIZE];
    char value[DOGESCRIPT_VARIABLE_VALUE_SIZE];
} dogescript_variable_t;

typedef enum {
    LOOP_WHILE,
    LOOP_REPEAT
} loop_kind_t;

typedef struct {
    loop_kind_t kind;
    uint32_t start_offset;
    uint32_t body_offset;
    uint32_t end_offset;
    uint32_t after_end_offset;
    uint64_t remaining;
} loop_frame_t;

typedef struct {
    const char *path;
    uint64_t file_size;
    uint64_t cache_start;
    size_t cache_length;
    uint8_t cache[DOGESCRIPT_READ_BUFFER_SIZE];
} script_reader_t;

static dogescript_variable_t variables[DOGESCRIPT_MAX_VARIABLES]
    __attribute__((section(".data.dogescript"))) = {{{1}, {1}}};
static size_t variable_count
    __attribute__((section(".data.dogescript"))) = 1;
static int active_argc __attribute__((section(".data.dogescript"))) = 1;
static char **active_argv __attribute__((section(".data.dogescript"))) =
    (char **)1;
static const char *active_script_path
    __attribute__((section(".data.dogescript"))) = (const char *)1;
static char token_storage[DOGESCRIPT_MAX_LINE_SIZE]
    __attribute__((section(".data.dogescript"))) = {1};
static char expanded_tokens[DOGESCRIPT_MAX_TOKENS]
                           [DOGESCRIPT_VARIABLE_VALUE_SIZE]
    __attribute__((section(".data.dogescript"))) = {{1}};

static size_t text_length(const char *text) {
    size_t length = 0;
    if (text == NULL) {
        return 0;
    }
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static int text_equal(const char *left, const char *right) {
    return str_strcmp(left, right) == 0;
}

static int is_space(char character) {
    return character == ' ' || character == '\t' || character == '\r';
}

static size_t append_text(char *destination, size_t position,
                          const char *text) {
    while (*text != '\0') {
        destination[position++] = *text++;
    }
    return position;
}

static int append_checked(char *destination, size_t capacity,
                          size_t *position, char character) {
    if (*position + 1 >= capacity) {
        return -1;
    }
    destination[(*position)++] = character;
    destination[*position] = '\0';
    return 0;
}

static int copy_text(char *destination, size_t capacity,
                     const char *source) {
    if (destination == NULL || source == NULL || capacity == 0) {
        return -1;
    }
    size_t length = text_length(source);
    if (length >= capacity) {
        return -1;
    }
    for (size_t i = 0; i <= length; i++) {
        destination[i] = source[i];
    }
    return 0;
}

static void clear_text(char *buffer, size_t capacity) {
    for (size_t i = 0; i < capacity; i++) {
        buffer[i] = '\0';
    }
}

static int variable_name_valid(const char *name) {
    if (name[0] == '\0' ||
        !((name[0] >= 'a' && name[0] <= 'z') ||
          (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_')) {
        return 0;
    }
    for (size_t i = 1; name[i] != '\0'; i++) {
        if (!((name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= 'A' && name[i] <= 'Z') ||
              (name[i] >= '0' && name[i] <= '9') || name[i] == '_')) {
            return 0;
        }
    }
    return 1;
}

static const char *get_variable(const char *name, int argc, char **argv,
                                const char *script_path) {
    static char argument_text[16]
        __attribute__((section(".data.dogescript"))) = {1};
    if (text_equal(name, "0")) {
        return script_path;
    }
    if (text_equal(name, "argc")) {
        str_u64toa((uint64_t)(argc - 1), argument_text);
        return argument_text;
    }
    if (name[0] >= '1' && name[0] <= '9') {
        uint64_t index = 0;
        for (size_t i = 0; name[i] != '\0'; i++) {
            if (name[i] < '0' || name[i] > '9') {
                index = 0;
                break;
            }
            index = index * 10 + (uint64_t)(name[i] - '0');
            if (index > (uint64_t)argc) {
                break;
            }
        }
        if (index > 0 && index < (uint64_t)argc && argv[index] != NULL) {
            return argv[index];
        }
        return "";
    }
    for (size_t i = 0; i < variable_count; i++) {
        if (text_equal(name, variables[i].name)) {
            return variables[i].value;
        }
    }
    return "";
}

static int set_variable(const char *name, const char *value) {
    if (!variable_name_valid(name) ||
        text_length(value) >= DOGESCRIPT_VARIABLE_VALUE_SIZE) {
        return -1;
    }
    for (size_t i = 0; i < variable_count; i++) {
        if (text_equal(name, variables[i].name)) {
            return copy_text(variables[i].value,
                             sizeof(variables[i].value), value);
        }
    }
    if (variable_count == DOGESCRIPT_MAX_VARIABLES) {
        return -1;
    }
    if (copy_text(variables[variable_count].name,
                  sizeof(variables[variable_count].name), name) != 0 ||
        copy_text(variables[variable_count].value,
                  sizeof(variables[variable_count].value), value) != 0) {
        return -1;
    }
    variable_count++;
    return 0;
}

static int unset_variable(const char *name) {
    for (size_t i = 0; i < variable_count; i++) {
        if (text_equal(name, variables[i].name)) {
            for (size_t j = i + 1; j < variable_count; j++) {
                (void)copy_text(variables[j - 1].name,
                                sizeof(variables[j - 1].name),
                                variables[j].name);
                (void)copy_text(variables[j - 1].value,
                                sizeof(variables[j - 1].value),
                                variables[j].value);
            }
            variable_count--;
            return 0;
        }
    }
    return 0;
}

static int expand_text(const char *source, char *destination,
                       size_t capacity, int argc, char **argv,
                       const char *script_path) {
    size_t position = 0;
    destination[0] = '\0';
    for (size_t i = 0; source[i] != '\0';) {
        if (source[i] != '$') {
            if (append_checked(destination, capacity, &position,
                               source[i++]) != 0) {
                return -1;
            }
            continue;
        }
        i++;
        char name[DOGESCRIPT_VARIABLE_NAME_SIZE];
        size_t name_length = 0;
        if (source[i] == '{') {
            i++;
            while (source[i] != '\0' && source[i] != '}') {
                if (name_length + 1 >= sizeof(name)) {
                    return -1;
                }
                name[name_length++] = source[i++];
            }
            if (source[i] != '}') {
                return -1;
            }
            i++;
        } else if ((source[i] >= '0' && source[i] <= '9') ||
                   (source[i] >= 'a' && source[i] <= 'z') ||
                   (source[i] >= 'A' && source[i] <= 'Z') ||
                   source[i] == '_') {
            while ((source[i] >= '0' && source[i] <= '9') ||
                   (source[i] >= 'a' && source[i] <= 'z') ||
                   (source[i] >= 'A' && source[i] <= 'Z') ||
                   source[i] == '_') {
                if (name_length + 1 >= sizeof(name)) {
                    return -1;
                }
                name[name_length++] = source[i++];
            }
        } else {
            if (append_checked(destination, capacity, &position, '$') != 0) {
                return -1;
            }
            continue;
        }
        name[name_length] = '\0';
        const char *value = get_variable(name, argc, argv, script_path);
        size_t value_length = text_length(value);
        if (value_length >= capacity - position) {
            return -1;
        }
        for (size_t j = 0; j < value_length; j++) {
            destination[position++] = value[j];
        }
        destination[position] = '\0';
    }
    return 0;
}

static int tokenize_line(const char *line, char **tokens, size_t *count) {
    size_t read_index = 0;
    size_t write_index = 0;
    *count = 0;
    while (line[read_index] != '\0') {
        char quote = '\0';
        while (is_space(line[read_index])) {
            read_index++;
        }
        if (line[read_index] == '\0') {
            break;
        }
        if (*count == DOGESCRIPT_MAX_TOKENS ||
            write_index >= sizeof(token_storage)) {
            return -1;
        }
        tokens[(*count)++] = token_storage + write_index;
        while (line[read_index] != '\0') {
            char character = line[read_index];
            if (quote == '\0' && is_space(character)) {
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
            if (write_index + 1 >= sizeof(token_storage)) {
                return -1;
            }
            token_storage[write_index++] = character;
        }
        if (quote != '\0' || write_index >= sizeof(token_storage)) {
            return -1;
        }
        token_storage[write_index++] = '\0';
    }
    for (size_t i = 0; i < *count; i++) {
        char *source = tokens[i];
        if (expand_text(source, expanded_tokens[i],
                        sizeof(expanded_tokens[i]), active_argc, active_argv,
                        active_script_path) != 0) {
            return -1;
        }
        tokens[i] = expanded_tokens[i];
    }
    return 0;
}

static int parse_integer(const char *text, int64_t *value) {
    size_t index = 0;
    int negative = 0;
    uint64_t number = 0;
    uint64_t limit;
    if (text[index] == '-' || text[index] == '+') {
        negative = text[index] == '-';
        index++;
    }
    if (text[index] == '\0') {
        return -1;
    }
    limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    for (; text[index] != '\0'; index++) {
        if (text[index] < '0' || text[index] > '9') {
            return -1;
        }
        uint64_t digit = (uint64_t)(text[index] - '0');
        if (number > (limit - digit) / 10) {
            return -1;
        }
        number = number * 10 + digit;
    }
    if (negative && number == (uint64_t)INT64_MAX + 1) {
        *value = INT64_MIN;
    } else if (negative) {
        *value = -(int64_t)number;
    } else {
        *value = (int64_t)number;
    }
    return 0;
}

static size_t format_integer(char *buffer, size_t capacity, int64_t value) {
    uint64_t magnitude = value < 0
                             ? (uint64_t)(-(value + 1)) + 1
                             : (uint64_t)value;
    char digits[21];
    str_u64toa(magnitude, digits);
    size_t position = 0;
    if (value < 0 && position + 1 < capacity) {
        buffer[position++] = '-';
    }
    for (size_t i = 0; digits[i] != '\0' && position + 1 < capacity; i++) {
        buffer[position++] = digits[i];
    }
    buffer[position] = '\0';
    return position;
}

static int compare_values(const char *left, const char *right) {
    int64_t left_number;
    int64_t right_number;
    if (parse_integer(left, &left_number) == 0 &&
        parse_integer(right, &right_number) == 0) {
        return left_number < right_number ? -1
               : left_number > right_number ? 1
                                            : 0;
    }
    return str_strcmp(left, right);
}

static int condition_value(size_t count, char **tokens, size_t first,
                           int *value) {
    dogec_stat_t info;
    if (first < count && text_equal(tokens[first], "exists")) {
        if (first + 2 != count) {
            return -1;
        }
        *value = (int64_t)file_exists(tokens[first + 1]) == 1;
        return 0;
    }
    if (first < count && text_equal(tokens[first], "isdir")) {
        if (first + 2 != count) {
            return -1;
        }
        info.size = 0;
        info.is_dir = 0;
        info.exists = 0;
        *value = (int64_t)stat(tokens[first + 1], &info) >= 0 &&
                 info.is_dir != 0;
        return 0;
    }
    if (count - first != 3) {
        return -1;
    }
    int comparison = compare_values(tokens[first], tokens[first + 2]);
    if (text_equal(tokens[first + 1], "==")) {
        *value = comparison == 0;
    } else if (text_equal(tokens[first + 1], "!=")) {
        *value = comparison != 0;
    } else if (text_equal(tokens[first + 1], "<")) {
        *value = comparison < 0;
    } else if (text_equal(tokens[first + 1], "<=")) {
        *value = comparison <= 0;
    } else if (text_equal(tokens[first + 1], ">")) {
        *value = comparison > 0;
    } else if (text_equal(tokens[first + 1], ">=")) {
        *value = comparison >= 0;
    } else {
        return -1;
    }
    return 0;
}

static int read_script_byte(script_reader_t *reader, uint64_t offset,
                            uint8_t *value) {
    if (offset >= reader->file_size) {
        return 0;
    }
    if (offset < reader->cache_start ||
        offset - reader->cache_start >= reader->cache_length) {
        uint64_t chunk_start =
            offset - (offset % (uint64_t)sizeof(reader->cache));
        uint64_t amount = reader->file_size - chunk_start;
        if (amount > sizeof(reader->cache)) {
            amount = sizeof(reader->cache);
        }
        int64_t bytes_read = (int64_t)read_file_at(
            reader->path, reader->cache, chunk_start, amount);
        if (bytes_read < 0 || (uint64_t)bytes_read != amount) {
            return -1;
        }
        reader->cache_start = chunk_start;
        reader->cache_length = (size_t)amount;
    }
    *value = reader->cache[(size_t)(offset - reader->cache_start)];
    return 1;
}

static int get_line(script_reader_t *reader, uint64_t offset, char *line,
                    uint64_t *next_offset) {
    size_t length = 0;
    uint64_t position = offset;
    while (position < reader->file_size) {
        uint8_t character;
        int result = read_script_byte(reader, position, &character);
        if (result != 1) {
            println("dogescript: unable to read script");
            return -1;
        }
        if (character == '\0') {
            println("dogescript: script contains a null byte");
            return -1;
        }
        position++;
        if (character == '\n') {
            break;
        }
        if (length + 1 >= DOGESCRIPT_MAX_LINE_SIZE) {
            println("dogescript: line exceeds the 255-character limit");
            return -1;
        }
        line[length++] = (char)character;
    }
    if (length > 0 && line[length - 1] == '\r') {
        length--;
    }
    line[length] = '\0';
    *next_offset = position;
    return 0;
}

static int get_tokens(script_reader_t *reader, uint64_t offset, char *line,
                      char **tokens, size_t *count, uint64_t *next_offset) {
    if (get_line(reader, offset, line, next_offset) != 0) {
        return -1;
    }
    size_t first = 0;
    while (is_space(line[first])) {
        first++;
    }
    if (line[first] == '\0' || line[first] == '#') {
        *count = 0;
        return 0;
    }
    return tokenize_line(line + first, tokens, count);
}

static uint64_t get_line_number(script_reader_t *reader, uint64_t offset) {
    uint64_t line_number = 1;
    for (uint64_t position = 0; position < offset; position++) {
        uint8_t character;
        if (read_script_byte(reader, position, &character) != 1) {
            return 0;
        }
        if (character == '\n') {
            line_number++;
        }
    }
    return line_number;
}

static int find_control_end(script_reader_t *reader, uint64_t start,
                            const char *opening, const char *closing,
                            uint64_t *end, uint64_t *after_end) {
    char line[DOGESCRIPT_MAX_LINE_SIZE];
    char *tokens[DOGESCRIPT_MAX_TOKENS];
    size_t count;
    size_t depth = 0;
    uint64_t position;
    if (get_line(reader, start, line, &position) != 0) {
        return -1;
    }
    while (position < reader->file_size) {
        uint64_t next_position;
        if (get_tokens(reader, position, line, tokens, &count,
                       &next_position) != 0) {
            return -1;
        }
        if (count == 0) {
            position = next_position;
            continue;
        }
        if (text_equal(tokens[0], opening)) {
            depth++;
        } else if (text_equal(tokens[0], closing)) {
            if (depth == 0) {
                *end = position;
                *after_end = next_position;
                return 0;
            }
            depth--;
        }
        position = next_position;
    }
    return -1;
}

static int find_if_end(script_reader_t *reader, uint64_t start, uint64_t *end,
                       uint64_t *after_end, uint64_t *else_position,
                       uint64_t *after_else) {
    char line[DOGESCRIPT_MAX_LINE_SIZE];
    char *tokens[DOGESCRIPT_MAX_TOKENS];
    size_t count;
    size_t depth = 0;
    uint64_t position;
    if (get_line(reader, start, line, &position) != 0) {
        return -1;
    }
    *else_position = UINT64_MAX;
    while (position < reader->file_size) {
        uint64_t next_position;
        if (get_tokens(reader, position, line, tokens, &count,
                       &next_position) != 0) {
            return -1;
        }
        if (count == 0) {
            position = next_position;
            continue;
        }
        if (text_equal(tokens[0], "if")) {
            depth++;
        } else if (text_equal(tokens[0], "endif")) {
            if (depth == 0) {
                *end = position;
                *after_end = next_position;
                return 0;
            }
            depth--;
        } else if (text_equal(tokens[0], "else") && depth == 0) {
            if (*else_position != UINT64_MAX) {
                return -1;
            }
            *else_position = position;
            *after_else = next_position;
        }
        position = next_position;
    }
    return -1;
}

static int join_tokens(char *output, size_t capacity, size_t *length,
                       size_t count, char **tokens, size_t first) {
    *length = 0;
    output[0] = '\0';
    for (size_t i = first; i < count; i++) {
        if (i != first &&
            append_checked(output, capacity, length, ' ') != 0) {
            return -1;
        }
        for (size_t j = 0; tokens[i][j] != '\0'; j++) {
            if (append_checked(output, capacity, length,
                               tokens[i][j]) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int build_shell_command(char *output, size_t capacity,
                               size_t count, char **tokens) {
    size_t position = 0;
    output[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        if (i != 0 && append_checked(output, capacity, &position, ' ') != 0) {
            return -1;
        }
        if (append_checked(output, capacity, &position, '"') != 0) {
            return -1;
        }
        for (size_t j = 0; tokens[i][j] != '\0'; j++) {
            if ((tokens[i][j] == '"' || tokens[i][j] == '\\') &&
                append_checked(output, capacity, &position, '\\') != 0) {
                return -1;
            }
            if (append_checked(output, capacity, &position,
                               tokens[i][j]) != 0) {
                return -1;
            }
        }
        if (append_checked(output, capacity, &position, '"') != 0) {
            return -1;
        }
    }
    return 0;
}

static int run_command(size_t count, char **tokens, int argc, char **argv,
                       const char *script_path, char *source_line,
                       int *exit_requested, int *exit_status) {
    char value[DOGESCRIPT_VARIABLE_VALUE_SIZE];
    clear_text(value, sizeof(value));
    size_t value_length;
    int64_t number;
    if (count == 0) {
        return 0;
    }
    if (text_equal(tokens[0], "echo") || text_equal(tokens[0], "print") ||
        text_equal(tokens[0], "println")) {
        if (join_tokens(value, sizeof(value), &value_length, count, tokens,
                        1) != 0) {
            println("dogescript: output is too long");
            return 1;
        }
        if (text_equal(tokens[0], "println")) {
            println(value);
        } else {
            print(value);
            if (text_equal(tokens[0], "echo")) {
                println("");
            }
        }
        return 0;
    }
    if (text_equal(tokens[0], "set")) {
        if (count < 3 ||
            join_tokens(value, sizeof(value), &value_length, count, tokens,
                        2) != 0 ||
            set_variable(tokens[1], value) != 0) {
            println("dogescript: usage: set <name> <value>");
            return 1;
        }
        return 0;
    }
    if (text_equal(tokens[0], "unset")) {
        if (count != 2) {
            println("dogescript: usage: unset <name>");
            return 1;
        }
        unset_variable(tokens[1]);
        return 0;
    }
    if (text_equal(tokens[0], "input")) {
        if (count < 2 || count > 3) {
            println("dogescript: usage: input <name> [prompt]");
            return 1;
        }
        const char *prompt = count == 3 ? tokens[2] : "";
        if ((int64_t)input(prompt, value, sizeof(value)) < 0 ||
            set_variable(tokens[1], value) != 0) {
            println("dogescript: input failed or variable is invalid");
            return 1;
        }
        return 0;
    }
    if (text_equal(tokens[0], "inc") || text_equal(tokens[0], "dec")) {
        if (count != 2) {
            println("dogescript: usage: inc|dec <variable>");
            return 1;
        }
        const char *old_value = get_variable(tokens[1], argc, argv,
                                             script_path);
        if (parse_integer(old_value, &number) != 0 ||
            (text_equal(tokens[0], "inc") && number == INT64_MAX) ||
            (text_equal(tokens[0], "dec") && number == INT64_MIN)) {
            println("dogescript: variable is not a valid incrementable integer");
            return 1;
        }
        number += text_equal(tokens[0], "inc") ? 1 : -1;
        (void)format_integer(value, sizeof(value), number);
        return set_variable(tokens[1], value) == 0 ? 0 : 1;
    }
    if (text_equal(tokens[0], "math")) {
        int64_t left;
        int64_t right;
        if (count != 5 || parse_integer(tokens[2], &left) != 0 ||
            parse_integer(tokens[4], &right) != 0) {
            println("dogescript: usage: math <name> <integer> <+|-|*|/> <integer>");
            return 1;
        }
        if (text_equal(tokens[3], "+")) {
            if ((right > 0 && left > INT64_MAX - right) ||
                (right < 0 && left < INT64_MIN - right)) {
                println("dogescript: integer overflow");
                return 1;
            }
            number = left + right;
        } else if (text_equal(tokens[3], "-")) {
            if ((right < 0 && left > INT64_MAX + right) ||
                (right > 0 && left < INT64_MIN + right)) {
                println("dogescript: integer overflow");
                return 1;
            }
            number = left - right;
        } else if (text_equal(tokens[3], "*")) {
            if ((left == INT64_MIN && right == -1) ||
                (right == INT64_MIN && left == -1)) {
                println("dogescript: integer overflow");
                return 1;
            }
            if ((left > 0 && right > 0 && left > INT64_MAX / right) ||
                (left > 0 && right < 0 && right < INT64_MIN / left) ||
                (left < 0 && right > 0 && left < INT64_MIN / right) ||
                (left < 0 && right < 0 && left < INT64_MAX / right)) {
                println("dogescript: integer overflow");
                return 1;
            }
            number = left * right;
        } else if (text_equal(tokens[3], "/")) {
            if (right == 0 || (left == INT64_MIN && right == -1)) {
                println("dogescript: invalid division");
                return 1;
            }
            number = left / right;
        } else {
            println("dogescript: unsupported math operator");
            return 1;
        }
        (void)format_integer(value, sizeof(value), number);
        return set_variable(tokens[1], value) == 0 ? 0 : 1;
    }
    if (text_equal(tokens[0], "exists") || text_equal(tokens[0], "isdir")) {
        dogec_stat_t info;
        if (count != 4 || !text_equal(tokens[2], "->")) {
            println("dogescript: usage: exists|isdir <path> -> <variable>");
            return 1;
        }
        info.size = 0;
        info.is_dir = 0;
        info.exists = 0;
        int64_t result = (int64_t)stat(tokens[1], &info);
        if (text_equal(tokens[0], "exists")) {
            number = result >= 0 && info.exists != 0 ? 1 : 0;
        } else {
            number = result >= 0 && info.is_dir != 0 ? 1 : 0;
        }
        (void)format_integer(value, sizeof(value), number);
        return set_variable(tokens[3], value) == 0 ? 0 : 1;
    }
    if (text_equal(tokens[0], "read")) {
        dogec_stat_t info;
        if (count != 3) {
            println("dogescript: usage: read <variable> <file>");
            return 1;
        }
        info.size = 0;
        info.is_dir = 0;
        info.exists = 0;
        if ((int64_t)stat(tokens[2], &info) < 0 || info.is_dir != 0 ||
            info.size >= sizeof(value)) {
            println("dogescript: unable to read file or file exceeds 255 bytes");
            return 1;
        }
        int64_t bytes_read = (int64_t)read_file(tokens[2], value, info.size);
        if (bytes_read < 0 || (uint64_t)bytes_read != info.size) {
            println("dogescript: unable to read complete file");
            return 1;
        }
        for (size_t i = 0; i < (size_t)info.size; i++) {
            if (value[i] == '\0') {
                println("dogescript: cannot store binary data in a text variable");
                return 1;
            }
        }
        value[info.size] = '\0';
        return set_variable(tokens[1], value) == 0 ? 0 : 1;
    }
    if (text_equal(tokens[0], "write") || text_equal(tokens[0], "append")) {
        if (count < 3 ||
            join_tokens(value, sizeof(value), &value_length, count, tokens,
                        2) != 0) {
            println("dogescript: usage: write|append <file> <text>");
            return 1;
        }
        int64_t result = text_equal(tokens[0], "write")
                             ? (int64_t)write_file(tokens[1], value)
                             : (int64_t)append_file(tokens[1], value,
                                                   (uint64_t)value_length);
        if (result < 0) {
            println("dogescript: file operation failed");
            return 1;
        }
        return 0;
    }
    if (text_equal(tokens[0], "assert")) {
        int condition;
        if (condition_value(count, tokens, 1, &condition) != 0) {
            println("dogescript: usage: assert <left> <operator> <right>");
            return 1;
        }
        if (!condition) {
            println("dogescript: assertion failed");
            return 1;
        }
        return 0;
    }
    if (text_equal(tokens[0], "fail")) {
        if (count == 1) {
            println("dogescript: fail");
        } else {
            if (join_tokens(value, sizeof(value), &value_length, count,
                            tokens, 1) != 0) {
                println("dogescript: failure message is too long");
                return 1;
            }
            println(value);
        }
        return 1;
    }
    if (text_equal(tokens[0], "exit")) {
        if (count > 2 ||
            (count == 2 && parse_integer(tokens[1], &number) != 0) ||
            (count == 2 && (number < 0 || number > 255))) {
            println("dogescript: usage: exit [0-255]");
            return 1;
        }
        *exit_requested = 1;
        *exit_status = count == 2 ? (int)number : 0;
        return 0;
    }

    int has_variable = 0;
    for (size_t i = 0; source_line[i] != '\0'; i++) {
        if (source_line[i] == '$') {
            has_variable = 1;
            break;
        }
    }
    if (!has_variable) {
        return (int64_t)shell(source_line) == 0 ? 0 : 1;
    }
    char shell_command[DOGESCRIPT_MAX_SHELL_COMMAND_SIZE];
    if (build_shell_command(shell_command, sizeof(shell_command), count,
                            tokens) != 0) {
        println("dogescript: expanded command is too long");
        return 1;
    }
    uint64_t result = shell(shell_command);
    return (int64_t)result == 0 ? 0 : 1;
}

static int run_script(const char *path, int argc, char **argv) {
    variable_count = 0;
    active_argc = argc;
    active_argv = argv;
    active_script_path = path;
    char resolved_path[256];
    if (path[0] == '/') {
        if (copy_text(resolved_path, sizeof(resolved_path), path) != 0) {
            println("dogescript: script path is too long");
            return 1;
        }
    } else {
        char working_directory[256];
        clear_text(working_directory, sizeof(working_directory));
        if ((int64_t)get_cwd(working_directory,
                             sizeof(working_directory)) < 0 ||
            working_directory[0] != '/' ||
            copy_text(resolved_path, sizeof(resolved_path),
                      working_directory) != 0) {
            println("dogescript: unable to resolve script path");
            return 1;
        }
        size_t path_length = text_length(resolved_path);
        if ((path_length > 1 &&
             append_checked(resolved_path, sizeof(resolved_path),
                            &path_length, '/') != 0)) {
            println("dogescript: script path is too long");
            return 1;
        }
        size_t script_path_length = text_length(path);
        if (path_length + script_path_length >= sizeof(resolved_path)) {
            println("dogescript: script path is too long");
            return 1;
        }
        for (size_t i = 0; i <= script_path_length; i++) {
            resolved_path[path_length + i] = path[i];
        }
    }
    dogec_stat_t file_stat;
    file_stat.size = 0;
    file_stat.is_dir = 0;
    file_stat.exists = 0;
    if ((int64_t)stat(resolved_path, &file_stat) < 0) {
        print("dogescript: unable to access ");
        println(path);
        return 1;
    }
    if (file_stat.is_dir != 0) {
        println("dogescript: path is a directory");
        return 1;
    }
    if (file_stat.size > DOGESCRIPT_MAX_SCRIPT_SIZE) {
        println("dogescript: script exceeds the 65535-byte size limit");
        return 1;
    }
    script_reader_t reader;
    reader.path = resolved_path;
    reader.file_size = file_stat.size;
    reader.cache_start = UINT64_MAX;
    reader.cache_length = 0;

    loop_frame_t loop_stack[DOGESCRIPT_MAX_LOOP_DEPTH];
    size_t loop_depth = 0;
    uint64_t program_counter = 0;
    size_t executed_steps = 0;
    int exit_requested = 0;
    int exit_status = 0;
    int64_t number;

    while (program_counter < reader.file_size && !exit_requested) {
        if (++executed_steps > DOGESCRIPT_MAX_STEPS) {
            println("dogescript: execution step limit exceeded");
            return 1;
        }
        char line[DOGESCRIPT_MAX_LINE_SIZE];
        char *tokens[DOGESCRIPT_MAX_TOKENS];
        size_t count;
        uint64_t next_offset;
        if (get_tokens(&reader, program_counter, line, tokens, &count,
                       &next_offset) != 0) {
            println("dogescript: invalid quoting, variable, or token count");
            return 1;
        }
        if (count == 0) {
            program_counter = next_offset;
            continue;
        }
        if (text_equal(tokens[0], "if") ||
            text_equal(tokens[0], "while")) {
            int condition;
            if (condition_value(count, tokens, 1, &condition) != 0) {
                println("dogescript: invalid condition");
                return 1;
            }
            if (text_equal(tokens[0], "if")) {
                uint64_t end_offset;
                uint64_t after_end;
                uint64_t else_offset;
                uint64_t after_else = UINT64_MAX;
                if (find_if_end(&reader, program_counter, &end_offset,
                                &after_end, &else_offset, &after_else) != 0) {
                    println("dogescript: missing or repeated endif/else");
                    return 1;
                }
                if (!condition) {
                    program_counter = else_offset != UINT64_MAX
                                          ? after_else
                                          : after_end;
                } else {
                    program_counter = next_offset;
                }
                continue;
            }
            uint64_t end_offset;
            uint64_t after_end;
            if (loop_depth > 0 &&
                loop_stack[loop_depth - 1].kind == LOOP_WHILE &&
                loop_stack[loop_depth - 1].start_offset == program_counter) {
                end_offset = loop_stack[loop_depth - 1].end_offset;
                after_end = loop_stack[loop_depth - 1].after_end_offset;
            } else if (find_control_end(&reader, program_counter, "while",
                                        "endwhile", &end_offset,
                                        &after_end) != 0) {
                println("dogescript: missing endwhile");
                return 1;
            }
            if (!condition) {
                if (loop_depth > 0 &&
                    loop_stack[loop_depth - 1].kind == LOOP_WHILE &&
                    loop_stack[loop_depth - 1].start_offset == program_counter) {
                    loop_depth--;
                }
                program_counter = after_end;
                continue;
            }
            if (loop_depth == 0 ||
                loop_stack[loop_depth - 1].start_offset != program_counter) {
                if (loop_depth == DOGESCRIPT_MAX_LOOP_DEPTH) {
                    println("dogescript: loop nesting limit exceeded");
                    return 1;
                }
                loop_stack[loop_depth].kind = LOOP_WHILE;
                loop_stack[loop_depth].start_offset = (uint32_t)program_counter;
                loop_stack[loop_depth].body_offset = (uint32_t)next_offset;
                loop_stack[loop_depth].end_offset = (uint32_t)end_offset;
                loop_stack[loop_depth].after_end_offset = (uint32_t)after_end;
                loop_stack[loop_depth].remaining = 0;
                loop_depth++;
            }
            program_counter = next_offset;
            continue;
        }
        if (text_equal(tokens[0], "repeat")) {
            uint64_t end_offset;
            uint64_t after_end;
            if (count != 2 || parse_integer(tokens[1], &number) != 0 ||
                number < 0 || (uint64_t)number > DOGESCRIPT_MAX_REPEAT) {
                println("dogescript: usage: repeat <count up to 100000>");
                return 1;
            }
            if (find_control_end(&reader, program_counter, "repeat",
                                 "endrepeat", &end_offset, &after_end) != 0) {
                println("dogescript: missing endrepeat");
                return 1;
            }
            if (number == 0) {
                program_counter = after_end;
                continue;
            }
            if (loop_depth == DOGESCRIPT_MAX_LOOP_DEPTH) {
                println("dogescript: loop nesting limit exceeded");
                return 1;
            }
            loop_stack[loop_depth].kind = LOOP_REPEAT;
            loop_stack[loop_depth].start_offset = (uint32_t)program_counter;
            loop_stack[loop_depth].body_offset = (uint32_t)next_offset;
            loop_stack[loop_depth].end_offset = (uint32_t)end_offset;
            loop_stack[loop_depth].after_end_offset = (uint32_t)after_end;
            loop_stack[loop_depth].remaining = (uint64_t)number;
            loop_depth++;
            program_counter = next_offset;
            continue;
        }
        if (text_equal(tokens[0], "else")) {
            uint64_t end_offset;
            uint64_t after_end;
            if (find_control_end(&reader, program_counter, "if", "endif",
                                 &end_offset, &after_end) != 0) {
                println("dogescript: unmatched else");
                return 1;
            }
            program_counter = after_end;
            continue;
        }
        if (text_equal(tokens[0], "endif")) {
            program_counter = next_offset;
            continue;
        }
        if (text_equal(tokens[0], "endwhile")) {
            if (loop_depth == 0 ||
                loop_stack[loop_depth - 1].kind != LOOP_WHILE ||
                loop_stack[loop_depth - 1].end_offset != program_counter) {
                println("dogescript: unmatched endwhile");
                return 1;
            }
            program_counter = loop_stack[loop_depth - 1].start_offset;
            continue;
        }
        if (text_equal(tokens[0], "endrepeat")) {
            if (loop_depth == 0 ||
                loop_stack[loop_depth - 1].kind != LOOP_REPEAT ||
                loop_stack[loop_depth - 1].end_offset != program_counter) {
                println("dogescript: unmatched endrepeat");
                return 1;
            }
            if (loop_stack[loop_depth - 1].remaining > 1) {
                loop_stack[loop_depth - 1].remaining--;
                program_counter = loop_stack[loop_depth - 1].body_offset;
            } else {
                loop_depth--;
                program_counter = next_offset;
            }
            continue;
        }
        if (text_equal(tokens[0], "break") ||
            text_equal(tokens[0], "continue")) {
            if (loop_depth == 0 || count != 1) {
                println("dogescript: break/continue used outside a loop");
                return 1;
            }
            loop_frame_t frame = loop_stack[loop_depth - 1];
            if (text_equal(tokens[0], "break")) {
                loop_depth--;
                program_counter = frame.after_end_offset;
            } else if (frame.kind == LOOP_WHILE) {
                program_counter = frame.start_offset;
            } else {
                program_counter = frame.end_offset;
            }
            continue;
        }

        int status = run_command(count, tokens, argc, argv, path, line,
                                 &exit_requested, &exit_status);
        if (status != 0) {
            char message[96] = "dogescript: command failed on line ";
            size_t message_length =
                sizeof("dogescript: command failed on line ") - 1;
            char number_text[21];
            uint64_t line_number = get_line_number(&reader, program_counter);
            if (line_number == 0) {
                println("dogescript: unable to determine failing line");
                return 1;
            }
            str_u64toa(line_number, number_text);
            message_length = append_text(message, message_length, number_text);
            message[message_length] = '\0';
            println(message);
            return 1;
        }
        program_counter = next_offset;
    }
    return exit_status;
}

void _start(int argc, char **argv) {
    if (argc == 1 && argv != NULL && argv[0] != NULL &&
        text_equal(argv[0], "--help")) {
        println("Usage: dogescript <script-file> [arguments...]");
        println("Commands: echo print println set unset input math inc dec");
        println("          read write append exists isdir assert fail exit");
        println("Flow:     if else endif while endwhile repeat endrepeat");
        println("          break continue. Other commands run in Dogeshell.");
        println("Variables: $name, ${name}, $0 (script), $1..$N, $argc");
        sys_exit(0);
        return;
    }
    if (argc < 1 || argv == NULL || argv[0] == NULL ||
        argv[0][0] == '\0') {
        println("Usage: dogescript <script-file> [arguments...]");
        sys_exit(2);
        return;
    }
    sys_exit((uint64_t)run_script(argv[0], argc, argv));
}
