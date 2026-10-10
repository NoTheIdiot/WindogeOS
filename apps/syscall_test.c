#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/stddef.h"

#define TEST_DIR_PREFIX "doge_syscall_test_"
#define TEST_SOURCE_NAME "source.txt"
#define TEST_COPY_NAME "copy.txt"
#define TEST_RENAMED_NAME "renamed.txt"
#define TEST_BUFFER_SIZE 64

static uint32_t failures;

static void report(const char *name, int passed) {
    print("[");
    print(passed ? "PASS" : "FAIL");
    print("] ");
    println(name);
    if (!passed) {
        failures++;
    }
}

static int result_succeeded(uint64_t result) {
    return (int64_t)result >= 0;
}

static int bytes_match(const char *actual, const char *expected, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (actual[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}

static int raw_bytes_match(const uint8_t *actual, const uint8_t *expected,
                           size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (actual[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}

static int has_name(char names[][256], uint64_t count, const char *expected) {
    for (uint64_t i = 0; i < count; i++) {
        if (str_strcmp(names[i], expected) == 0) {
            return 1;
        }
    }
    return 0;
}

static void make_path(char *path, const char *directory, const char *filename) {
    str_strcpy(path, directory);
    str_strcat(path, "/");
    str_strcat(path, filename);
}

void _start(void) {
    char test_dir[48];
    char source_path[80];
    char copy_path[80];
    char renamed_path[80];
    char suffix[4];
    char file_buffer[TEST_BUFFER_SIZE] = {0};
    uint8_t raw_file_buffer[TEST_BUFFER_SIZE] = {0};
    uint8_t raw_offset_buffer[3] = {0};
    const uint8_t raw_test_data[] = {0x41, 0x00, 0xFF, 0x42, 0x7F};
    char input_buffer[TEST_BUFFER_SIZE] = {0};
    char cwd_buffer[256] = {0};
    int have_test_dir = 0;
    int have_source = 0;
    int have_copy = 0;
    int have_renamed = 0;
    int have_moved = 0;

    uint64_t clear_result = clear();
    report("CLEAR", result_succeeded(clear_result));
    println("WindogeOS syscall test");
    println("Filesystem tests use a temporary directory.");
    println("");

    int available_name_found = 0;
    for (int candidate = 0; candidate < 100; candidate++) {
        str_strcpy(test_dir, TEST_DIR_PREFIX);
        str_itoa(candidate, suffix);
        str_strcat(test_dir, suffix);
        if (!file_exists(test_dir)) {
            available_name_found = 1;
            break;
        }
    }
    report("FILE_EXISTS (unused temporary name)", available_name_found);

    uint64_t mkdir_result = available_name_found ? create_dir(test_dir) : (uint64_t)-1;
    have_test_dir = available_name_found && mkdir_result == 0;
    report("CREATE_DIR", have_test_dir);

    if (have_test_dir) {
        make_path(source_path, test_dir, TEST_SOURCE_NAME);
        make_path(copy_path, test_dir, TEST_COPY_NAME);
        make_path(renamed_path, test_dir, TEST_RENAMED_NAME);

        uint64_t create_result = create_file(source_path);
        have_source = create_result == 0;
        report("CREATE_FILE", have_source);

        uint64_t exists_result = file_exists(source_path);
        report("FILE_EXISTS (created file)", exists_result == 1);

        uint64_t write_result = have_source
            ? write_file(source_path, "first\nsecond\n")
            : (uint64_t)-1;
        report("WRITE_FILE", result_succeeded(write_result));

        memset(file_buffer, 0, sizeof(file_buffer));
        uint64_t read_result = result_succeeded(write_result)
            ? read_file(source_path, file_buffer, sizeof(file_buffer))
            : (uint64_t)-1;
        int read_ok = result_succeeded(read_result) &&
                      read_result == 13 &&
                      bytes_match(file_buffer, "first\nsecond\n", 13);
        report("READ_FILE", read_ok);

        uint64_t raw_write_result = read_ok
            ? write_file_raw(source_path, raw_test_data,
                             sizeof(raw_test_data))
            : (uint64_t)-1;
        int raw_write_ok = raw_write_result == 0;
        report("WRITE_FILE_RAW (binary bytes)", raw_write_ok);

        uint64_t raw_read_result = raw_write_ok
            ? read_file_raw(source_path, raw_file_buffer,
                            sizeof(raw_file_buffer))
            : (uint64_t)-1;
        int raw_read_ok = result_succeeded(raw_read_result) &&
                          raw_read_result == sizeof(raw_test_data) &&
                          raw_bytes_match(raw_file_buffer, raw_test_data,
                                          sizeof(raw_test_data));
        report("READ_FILE_RAW (binary bytes)", raw_read_ok);

        uint64_t raw_read_at_result = raw_read_ok
            ? read_file_raw_at(source_path, raw_offset_buffer, 2,
                               sizeof(raw_offset_buffer))
            : (uint64_t)-1;
        const uint8_t raw_offset_expected[] = {0xFF, 0x42, 0x7F};
        int raw_read_at_ok = result_succeeded(raw_read_at_result) &&
                             raw_read_at_result ==
                                 sizeof(raw_offset_buffer) &&
                             raw_bytes_match(raw_offset_buffer,
                                             raw_offset_expected,
                                             sizeof(raw_offset_expected));
        report("READ_FILE_RAW_AT (binary bytes)", raw_read_at_ok);

        uint64_t restore_result = raw_write_ok
            ? write_file(source_path, "first\nsecond\n")
            : (uint64_t)-1;
        report("WRITE_FILE restore text", result_succeeded(restore_result));

        dogec_stat_t source_stat;
        dogec_stat_t directory_stat;
        source_stat.size = 0;
        source_stat.is_dir = 0;
        source_stat.exists = 0;
        directory_stat.size = 0;
        directory_stat.is_dir = 0;
        directory_stat.exists = 0;
        uint64_t source_stat_result = stat(source_path, &source_stat);
        uint64_t directory_stat_result = stat(test_dir, &directory_stat);
        report("STAT (file size and type)",
               result_succeeded(source_stat_result) &&
               source_stat.size == 13 && source_stat.is_dir == 0);
        report("STAT (directory type)",
               result_succeeded(directory_stat_result) &&
               directory_stat.is_dir == 1);

        uint64_t truncate_result = result_succeeded(read_result)
            ? delete_last_line(source_path)
            : (uint64_t)-1;
        report("DELETE_LAST_LINE", result_succeeded(truncate_result));

        memset(file_buffer, 0, sizeof(file_buffer));
        read_result = result_succeeded(truncate_result)
            ? read_file(source_path, file_buffer, sizeof(file_buffer))
            : (uint64_t)-1;
        int truncate_ok = result_succeeded(read_result) &&
                          read_result == 6 &&
                          bytes_match(file_buffer, "first\n", 6);
        report("DELETE_LAST_LINE result", truncate_ok);

        uint64_t append_result = truncate_ok
            ? append_file(source_path, "second\n", 7)
            : (uint64_t)-1;
        memset(file_buffer, 0, sizeof(file_buffer));
        read_result = result_succeeded(append_result)
            ? read_file(source_path, file_buffer, sizeof(file_buffer))
            : (uint64_t)-1;
        int append_ok = result_succeeded(read_result) &&
                        read_result == 13 &&
                        bytes_match(file_buffer, "first\nsecond\n", 13);
        report("APPEND_FILE", append_ok);

        memset(file_buffer, 0, sizeof(file_buffer));
        uint64_t read_at_result = append_ok
            ? read_file_at(source_path, file_buffer, 6, 7)
            : (uint64_t)-1;
        report("READ_FILE_AT",
               result_succeeded(read_at_result) && read_at_result == 7 &&
               bytes_match(file_buffer, "second\n", 7));

        uint64_t copy_result = append_ok
            ? copy_file(source_path, copy_path)
            : (uint64_t)-1;
        have_copy = result_succeeded(copy_result);
        report("COPY_FILE", have_copy);

        uint64_t rename_result = have_copy
            ? rename_file(copy_path, renamed_path)
            : (uint64_t)-1;
        have_renamed = rename_result == 1;
        have_copy = have_copy && !have_renamed;
        report("RENAME_FILE", have_renamed);

        uint64_t move_result = have_renamed
            ? move_file(renamed_path, copy_path)
            : (uint64_t)-1;
        have_moved = result_succeeded(move_result) &&
                     file_exists(copy_path) == 1 &&
                     file_exists(renamed_path) == 0;
        if (have_moved) {
            have_renamed = 0;
            have_copy = 1;
        }
        report("MOVE_FILE", have_moved);

        uint64_t change_result = change_dir(test_dir);
        int changed_into_test_dir = change_result == 0;
        uint64_t cwd_length = changed_into_test_dir
            ? get_cwd(cwd_buffer, sizeof(cwd_buffer))
            : (uint64_t)-1;
        int cwd_ok = (int64_t)cwd_length >= 0 &&
                     str_strcmp(cwd_buffer, test_dir) == 0;
        report("GET_CWD", cwd_ok);
        char listed_names[4][256];
        for (size_t i = 0; i < sizeof(listed_names) / sizeof(listed_names[0]);
             i++) {
            listed_names[i][0] = '\0';
        }
        uint64_t list_result = changed_into_test_dir
            ? list_dir(listed_names, 4)
            : (uint64_t)-1;
        int list_ok = result_succeeded(list_result) &&
                      list_result <=
                          sizeof(listed_names) / sizeof(listed_names[0]) &&
                      has_name(listed_names, list_result, TEST_SOURCE_NAME) &&
                      has_name(listed_names, list_result, TEST_COPY_NAME);
        report("LIST_DIR (files)", list_ok);
        if (changed_into_test_dir) {
            change_result = change_dir("..");
        }
        report("CHANGE_DIR", changed_into_test_dir && change_result == 0);

        if (have_source) {
            uint64_t delete_result = delete_file(source_path);
            report("DELETE_FILE (source)", delete_result == 1);
            have_source = delete_result != 1;
        } else {
            report("DELETE_FILE (source)", 0);
        }

        if (have_renamed) {
            uint64_t delete_result = delete_file(renamed_path);
            report("DELETE_FILE (renamed copy)", delete_result == 1);
            have_renamed = delete_result != 1;
        } else if (have_copy) {
            uint64_t delete_result = delete_file(copy_path);
            report("DELETE_FILE (moved file)", delete_result == 1);
            have_copy = delete_result != 1;
        } else {
            report("DELETE_FILE (moved file)", 0);
        }

        if (!have_source && !have_copy && !have_renamed) {
            uint64_t delete_dir_result = delete_file(test_dir);
            report("DELETE_FILE (temporary directory)", delete_dir_result == 1);
        } else {
            report("DELETE_FILE (temporary directory)", 0);
        }
    } else {
        report("CREATE_FILE", 0);
        report("FILE_EXISTS (created file)", 0);
        report("WRITE_FILE", 0);
        report("READ_FILE", 0);
        report("STAT (file size and type)", 0);
        report("STAT (directory type)", 0);
        report("DELETE_LAST_LINE", 0);
        report("DELETE_LAST_LINE result", 0);
        report("APPEND_FILE", 0);
        report("READ_FILE_AT", 0);
        report("COPY_FILE", 0);
        report("RENAME_FILE", 0);
        report("MOVE_FILE", 0);
        report("GET_CWD", 0);
        report("LIST_DIR (files)", 0);
        report("CHANGE_DIR", 0);
        report("DELETE_FILE (source)", 0);
        report("DELETE_FILE (moved file)", 0);
        report("DELETE_FILE (temporary directory)", 0);
    }

    println("");
    println("Console syscall checks:");
    report("PRINT", result_succeeded(print("PRINT syscall executed.\n")));
    report("PRINTLN", result_succeeded(println("PRINTLN syscall executed.")));

    uint64_t text_color_result = text_color(0xFFFF5555);
    report("TEXT_COLOR", result_succeeded(text_color_result));
    text_color(0xFFCCCCCC);
    uint64_t background_color_result = background_color(0x000040);
    report("BACKGROUND_COLOR", result_succeeded(background_color_result));
    background_color(0x000000);

    report("PRINT_AT",
           result_succeeded(print_at("PRINT_AT syscall executed.", 0, 2,
                                     0xFFCCCCCC)));

    println("INPUT test: type a short line, then press Enter.");
    uint64_t input_result = input("input> ", input_buffer, sizeof(input_buffer));
    int input_ok = (int64_t)input_result >= 0 &&
                   input_result < sizeof(input_buffer);
    report("INPUT", input_ok);

    println("GET_KEY test: press any supported key.");
    uint64_t key_result = get_key();
    report("GET_KEY", key_result != 0);

    println("WindogeOS syscall test finished.");

    if (failures == 0) {
        println("All syscall checks passed.");
        sys_exit(0);
    }

    print("Syscall checks failed: ");
    char failure_count[12];
    str_u64toa(failures, failure_count);
    println(failure_count);
    sys_exit(1);
}
