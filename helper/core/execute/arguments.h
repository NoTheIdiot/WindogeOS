#ifndef EXECUTE_ARGUMENTS_H
#define EXECUTE_ARGUMENTS_H

#include <stddef.h>
#include <stdint.h>

static inline int prepare_user_arguments(uint8_t *stack_memory,
                                         uint64_t user_stack_base,
                                         size_t argument_area_size,
                                         int argc, char **argv,
                                         uint64_t *user_argv) {
    uint64_t argument_addresses[32];
    size_t cursor = 0;

    if (argc < 0 || argc > 32 || (argc > 0 && argv == NULL)) {
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        if (argv[i] == NULL) {
            return -1;
        }
        size_t length = 0;
        while (length < 256 && argv[i][length] != '\0') {
            length++;
        }
        if (length == 256 || cursor > argument_area_size ||
            length + 1 > argument_area_size - cursor) {
            return -1;
        }
        argument_addresses[i] = user_stack_base + cursor;
        for (size_t j = 0; j < length; j++) {
            stack_memory[cursor + j] = (uint8_t)argv[i][j];
        }
        stack_memory[cursor + length] = '\0';
        cursor += length + 1;
    }

    cursor = (cursor + sizeof(uint64_t) - 1U) &
             ~(sizeof(uint64_t) - 1U);
    size_t vector_size = ((size_t)argc + 1U) * sizeof(uint64_t);
    if (cursor > argument_area_size ||
        vector_size > argument_area_size - cursor) {
        return -1;
    }
    uint64_t *vector = (uint64_t *)(stack_memory + cursor);
    for (int i = 0; i < argc; i++) {
        vector[i] = argument_addresses[i];
    }
    vector[argc] = 0;
    *user_argv = user_stack_base + cursor;
    return 0;
}

#endif
