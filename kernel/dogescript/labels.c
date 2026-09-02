#include <system.h>
#include <core.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <dogeio.h>
#include <bool.h>
#include "dogescript.h"

script_label label_table[MAX_LABELS];
size_t total_labels = 0;

int add_label(char* name, const char* ptr) {
    for (size_t i = 0; i < total_labels; i++) {
        if (str_strcmp(label_table[i].label_name, name) == 0) {
            label_table[i].script_ptr = ptr;
            return 1;
        }
    }

    if (total_labels >= MAX_LABELS) {
        return 0;
    }

    str_strncpy(label_table[total_labels].label_name, name, 15);
    label_table[total_labels].label_name[15] = '\0';
    label_table[total_labels].script_ptr = ptr;
    total_labels++;
    return 1;
}

const char* find_label(char* name) {
    for (size_t i = 0; i < total_labels; i++) {
        if (str_strcmp(label_table[i].label_name, name) == 0) {
            return label_table[i].script_ptr;
        }
    }
    return NULL;
}

void scan_labels(const char* script_text) {
    if (script_text == NULL) {
        total_labels = 0;
        return;
    }

    const char* line = script_text;
    total_labels = 0;

    while (*line != '\0') {
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') {
            line++;
        }
        if (*line == '\0') break;

            if (str_strncmp(line, "LABEL ", 6) == 0) {
            line += 6;
            char name[16];
            size_t i = 0;
            while (line[i] != ' ' && line[i] != '\n' && line[i] != '\r' && line[i] != '\0' && i < 15) {
                name[i] = line[i];
                i++;
            }
            name[i] = '\0';
            const char* target = line + i;
            while (*target != '\n' && *target != '\0') {
                target++;
            }
            if (*target == '\n') {
                target++;
            }
            add_label(name, target);
        }

        while (*line != '\n' && *line != '\0') {
            line++;
        }
    }
}