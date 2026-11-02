#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/stdint.h"
#include "../library/stddef.h"
#include "../library/bool.h"
#include "../library/math.h"

/*int search_text(char* path, char* text) {
    if (path == NULL || text == NULL) {
        return -1;
    }

    char* files[256];
    list_dir(files, 256);

    int file_count = 0;
    for (int i = 0; i < 256; i++) {
        if (files[i] == NULL) {
            break;
        }
        file_count++;
    }

    dogec_stat_t* thing[file_count];
    char temp[512];
    int total_results = 0;

    for (int i = 0; i < file_count; i++) {
        stat(files[i], thing[i]);
        char buffer[thing[i]->size];
        read_file(files[i], (char*)buffer, thing[i]->size);

        for (int j = 0; j < thing[i]->size; j++) {
            int found = 1;
            for (int k = 0; text[k] != '\0'; k++) {
                if ((j + k) >= thing[i]->size || buffer[j + k] != text[k]) {
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

int search_file(char* path, char* filename) {
    if (path == NULL || filename == NULL) {
        return -1;
    }
    return -1;
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
}*/

void _start(int argc, char** argv) {

    exit(0);
}