#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/stdint.h"
#include "../library/stddef.h"
#include "../library/string.h"
#include "../library/bool.h"
#include "../library/math.h"

int search_text(char* path, char* text) {
    if (path == NULL || text == NULL) {
        return -1;
    }

    char files[256][256];
    list_dir(files, 256);

    int file_count = 0;
    for (int i = 0; i < 256; i++) {
        if (files[i] == NULL) {
            break;
        }
        file_count++;
    }

    dogec_stat_t* thing[file_count];
    int total_results = 0;

    for (int i = 0; i < file_count; i++) {
        stat(files[i], thing[i]);
        char buffer[(int)thing[i]->size];
        read_file(files[i], (char*)buffer, thing[i]->size);

        for (int j = 0; j < (int)thing[i]->size; j++) {
            int found = 1;
            for (int k = 0; text[k] != '\0'; k++) {
                if ((j + k) >= (int)thing[i]->size || buffer[j + k] != text[k]) {
                    found = 0;
                    break;
                }
            }
            if (found == 1) {
                total_results++;
            }
        }
    }

    return total_results;
}

void sort(int* arr) {
    int size = *(((int*)arr) - 1);
    if (size < 2) return;

    int stack[64];
    int top = 0;

    stack[top++] = 0;
    stack[top++] = size - 1;

    while (top > 0) {
        int high = stack[--top];
        int low = stack[--top];

        int pivot = *(arr + high);
        int i = low - 1;

        for (int j = low; j < high; j++) {
            if (*(arr + j) <= pivot) {
                i++;
                int temp = *(arr + i);
                *(arr + i) = *(arr + j);
                *(arr + j) = temp;
            }
        }
        int pivot_index = i + 1;
        int temp = *(arr + pivot_index);
        *(arr + pivot_index) = *(arr + high);
        *(arr + high) = temp;

        if (pivot_index - 1 > low) {
            stack[top++] = low;
            stack[top++] = pivot_index - 1;
        }
        if (pivot_index + 1 < high) {
            stack[top++] = pivot_index + 1;
            stack[top++] = high;
        }
    }
}

int word_count(char* text) {
    if (text == NULL) {
        return -1;
    }

    int count = 0;
    int in_word = 0;

    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r') {
            in_word = 0;
        } else if (in_word == 0) {
            in_word = 1;
            count++;
        }
    }

    return count;
}

void _start(int argc, char** argv) {
    if (argc == 0 || argv == NULL) {
        exit((uint64_t)-1);
    }

    if (argc <= 1 && str_strcmp(argv[0], "search") == 0) {
        if (argv[1] == NULL) {
            exit((uint64_t)-2);
        }

        if (argv[2] == NULL) {
            exit((uint64_t)-2);
        }

        if (argv[3] == NULL) {
            exit((uint64_t)-2);
        }


        if (str_strcmp(argv[1], "text") == 0) {
            if (!file_exists(argv[2])) {
                exit((uint64_t)-3); 
            }

            int results = search_text(argv[2], argv[3]);
            char results_str[16];
            str_itoa(results, results_str);
            print(argv[2]);
            print(" found in ");
            print(argv[3]);
            print(" > ");
            println(results_str);
        }
    }
    exit((uint64_t)0);
}