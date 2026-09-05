#include <system.h>
#include <core.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <dogeio.h>
#include <bool.h>
#include <basicutil.h>
#include "dogescript.h"

#define DOGESCRIPT_LINE_SIZE 256
#define DOGESCRIPT_MAX_STEPS 10000

static void skip_spaces(const char **text) {
	if (text == NULL || *text == NULL) return;
	while (**text == ' ' || **text == '\t') {
		(*text)++;
	}
}

static int token_equals(const char *text, const char *token) {
	size_t index = 0;
	while (token[index] != '\0' && text[index] == token[index]) {
		index++;
	}
	return token[index] == '\0' && (text[index] == ' ' || text[index] == '\t' || text[index] == '\0');
}

static int parse_number(const char *text, uint64_t *value) {
	uint64_t result = 0;
	size_t index = 0;
	int base = 10;

	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		base = 16;
		index = 2;
	}
	if (text[index] == '\0') {
		return 0;
	}
	while (text[index] != '\0' && text[index] != ' ' && text[index] != '\t') {
		uint8_t digit;
		char current = text[index++];
		if (current >= '0' && current <= '9') digit = (uint8_t)(current - '0');
		else if (base == 16 && current >= 'a' && current <= 'f') digit = (uint8_t)(current - 'a' + 10);
		else if (base == 16 && current >= 'A' && current <= 'F') digit = (uint8_t)(current - 'A' + 10);
		else return 0;
		if (digit >= (uint8_t)base) return 0;
		result = result * (uint64_t)base + digit;
	}
	*value = result;
	return 1;
}

static int parse_value(const char *text, uint64_t *value) {
	if (text[0] == '$') {
		variable *found = get_variable((char *)(text + 1));
		if (found == NULL || found->is_string) return 0;
		*value = found->value;
		return 1;
	}
	return parse_number(text, value);
}

static size_t read_token(const char *text, char *token, size_t token_size) {
	size_t length = 0;
	skip_spaces(&text);
	while (*text != '\0' && *text != ' ' && *text != '\t' && *text != '=' && *text != '-' && length + 1 < token_size) {
		token[length++] = *text++;
	}
	token[length] = '\0';
	return length;
}

static const char *find_text(const char *text, const char *needle) {
	if (text == NULL || needle == NULL) return NULL;
	size_t needle_length = str_strlen(needle);
	while (*text != '\0') {
		if (str_strncmp(text, needle, needle_length) == 0) return text;
		text++;
	}
	return NULL;
}

static int parse_token_value(const char *text, uint64_t *value) {
	char token[DOGESCRIPT_LINE_SIZE];
	if (read_token(text, token, sizeof(token)) == 0) return 0;
	return parse_value(token, value);
}

static int set_numeric_variable_from_token(const char *name, const char *text) {
	uint64_t value;
	if (!parse_token_value(text, &value)) return 0;
	return set_variable((char *)name, value, NULL, false);
}

static void print_number(uint64_t value) {
	char buffer[21];
	size_t index = sizeof(buffer) - 1;
	buffer[index] = '\0';
	do {
		buffer[--index] = (char)('0' + (value % 10));
		value /= 10;
	} while (value != 0);
	dogeio_text_print(buffer + index);
}

static const char *next_line(const char *line) {
	while (*line != '\0' && *line != '\n') line++;
	return *line == '\n' ? line + 1 : line;
}

int system_dogescript_execute(const char* script_text) {
	if (script_text == NULL) return 0;
	scan_labels(script_text);

	const char *line = script_text;
	for (size_t steps = 0; *line != '\0' && steps < DOGESCRIPT_MAX_STEPS; steps++) {
		char command[DOGESCRIPT_LINE_SIZE];
		memset(command, 0, sizeof(command));
		size_t length = 0;
		while (line[length] != '\0' && line[length] != '\n' && line[length] != '\r' && length < sizeof(command) - 1) {
			command[length] = line[length];
			length++;
		}
		command[length] = '\0';
		const char *text = command;
		skip_spaces(&text);
		if (*text == '\0' || *text == ';') { line = next_line(line); continue; }

		if (token_equals(text, "HALT")) { halt(); return 1; }
		if (str_startswith(text, "LABEL ")) { line = next_line(line); continue; }

		if (str_startswith(text, "PRINT ")) {
			text += 6;
			if (*text == '"') {
				text++;
				while (*text != '"' && *text != '\0') dogeio_text_printchar(*text++);
			} else if (*text == '$') {
				variable *found = get_variable((char *)(text + 1));
				if (found != NULL) {
					if (found->is_string) dogeio_text_print(found->string_value);
					else print_number(found->value);
				}
			} else {
				uint64_t value;
				if (parse_value(text, &value)) print_number(value);
			}
			line = next_line(line); continue;
		}

		if (str_startswith(text, "VAR ")) {
			char name[16];
			char value_text[DOGESCRIPT_LINE_SIZE];
			const char *assignment = find_text(text + 4, "=");
			if (assignment != NULL && read_token(text + 4, name, sizeof(name)) != 0) {
				assignment++;
				skip_spaces(&assignment);
				if (*assignment == '"') {
					size_t value_length = 0;
					assignment++;
					while (assignment[value_length] != '"' && assignment[value_length] != '\0' && value_length + 1 < sizeof(value_text)) {
						value_text[value_length] = assignment[value_length];
						value_length++;
					}
					value_text[value_length] = '\0';
					set_variable(name, 0, value_text, true);
				} else {
					set_numeric_variable_from_token(name, assignment);
				}
			}
			line = next_line(line); continue;
		}

		if (str_startswith(text, "POKE ")) {
			uint64_t address, value;
			if (parse_token_value(text + 5, &address)) {
				const char *second = text + 5;
				while (*second != ' ' && *second != '\t' && *second != '\0') second++;
				skip_spaces(&second);
				if (parse_token_value(second, &value)) poke(address, value);
			}
			line = next_line(line); continue;
		}

		if (str_startswith(text, "PEEK ")) {
			uint64_t address, value;
			if (parse_token_value(text + 5, &address)) {
				const char *arrow = find_text(text + 5, "->");
				if (arrow != NULL && peek(address, &value)) {
					char name[16];
					if (read_token(arrow + 2, name, sizeof(name)) != 0) set_variable(name, value, NULL, false);
				}
			}
			line = next_line(line); continue;
		}

		if (str_startswith(text, "OUTB ")) {
			uint64_t port, value;
			if (parse_token_value(text + 5, &port)) {
				const char *second = text + 5;
				while (*second != ' ' && *second != '\t' && *second != '\0') second++;
				skip_spaces(&second);
				if (parse_token_value(second, &value)) ports_outb((uint16_t)port, (uint8_t)value);
			}
			line = next_line(line); continue;
		}

		if (str_startswith(text, "INB ")) {
			uint64_t port;
			if (parse_token_value(text + 4, &port)) {
				const char *arrow = find_text(text + 4, "->");
				if (arrow != NULL) {
					char name[16];
					if (read_token(arrow + 2, name, sizeof(name)) != 0) set_variable(name, ports_inb((uint16_t)port), NULL, false);
				}
			}
			line = next_line(line); continue;
		}

		if (str_startswith(text, "GOTO ")) {
                char name[16];
                read_token(text + 5, name, sizeof(name));
                const char *target = find_label(name);
                if (target == NULL) return 0;
                line = target; continue;
            }

            if (str_startswith(text, "IF ")) {
                char variable_name[16];
                char comparison[32];
                char label_name[16];
                const char *equals = find_text(text + 3, "==");
                const char *goto_text = find_text(text + 3, "GOTO ");
                if (equals != NULL && goto_text != NULL && read_token(text + 3, variable_name, sizeof(variable_name)) != 0) {
                    variable *found = get_variable(variable_name);
                    if (read_token(equals + 2, comparison, sizeof(comparison)) != 0 && read_token(goto_text + 5, label_name, sizeof(label_name)) != 0) {
                        uint64_t expected;
                        if (found != NULL && !found->is_string && parse_value(comparison, &expected) && found->value == expected) {
                            const char *target = find_label(label_name);
                            if (target == NULL) return 0;
                            line = target; continue;
                        }
                    }
                }
                line = next_line(line); continue;
            }
            line = next_line(line);
        }
        return *line == '\0';
}
