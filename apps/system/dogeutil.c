#include "../library/dogeio.h"
#include "../library/string.h"
#include "../library/stddef.h"
#include "../library/stdint.h"

#define DOGEUTIL_IO_SIZE 512
#define DOGEUTIL_LINE_SIZE 240
#define DOGEUTIL_SEARCH_SIZE 128
#define DOGEUTIL_LIST_CAPACITY 16

static int text_equal(const char *left, const char *right)
{
    return str_strcmp(left, right) == 0;
}

static int contains_text(const char *text, const char *fragment)
{
    size_t fragment_length = str_strlen(fragment);
    if (fragment_length == 0) {
        return 1;
    }

    for (size_t i = 0; text[i] != '\0'; i++) {
        if (str_strncmp(text + i, fragment, fragment_length) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_space(uint8_t value)
{
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\v' || value == '\f';
}

static void print_u64(uint64_t value)
{
    char text[21];
    str_u64toa(value, text);
    print(text);
}

static int usage(void)
{
    println("dogeutil - WindogeOS utility toolbox");
    println("Usage:");
    println("  dogeutil grep <text> <file>");
    println("  dogeutil find <name-fragment>");
    println("  dogeutil wc <file>");
    println("  dogeutil stat <path>");
    println("  dogeutil exists <path>");
    println("  dogeutil ls");
    println("  dogeutil random [byte-count]");
    println("  dogeutil time | user | cwd | uname");
    println("Other programs can call /system/dogeutil.elf with these arguments.");
    return 0;
}

static int read_chunk(const char *path, uint64_t offset, uint8_t *buffer,
                      uint64_t size)
{
    int64_t result = (int64_t)read_file_at(path, buffer, offset, size);
    if (result < 0 || (uint64_t)result != size) {
        print("dogeutil: read failed: ");
        println(path);
        return 0;
    }
    return 1;
}

static int command_grep(const char *needle, const char *path)
{
    size_t needle_length = str_strlen(needle);
    if (needle_length == 0 || needle_length >= DOGEUTIL_SEARCH_SIZE) {
        println("dogeutil: search text must contain 1 to 127 bytes");
        return 2;
    }

    dogec_stat_t file_info = {0};
    if ((int64_t)stat(path, &file_info) < 0 || file_info.exists == 0 ||
        file_info.is_dir != 0) {
        println("dogeutil: cannot search that file");
        return 1;
    }

    size_t prefix[DOGEUTIL_SEARCH_SIZE];
    prefix[0] = 0;
    for (size_t i = 1, matched = 0; i < needle_length; i++) {
        while (matched > 0 && needle[i] != needle[matched]) {
            matched = prefix[matched - 1];
        }
        if (needle[i] == needle[matched]) {
            matched++;
        }
        prefix[i] = matched;
    }

    uint8_t chunk[DOGEUTIL_IO_SIZE];
    char line[DOGEUTIL_LINE_SIZE + 1];
    size_t line_length = 0;
    size_t matched = 0;
    uint64_t line_number = 1;
    int line_found = 0;
    int line_matches = 0;

    for (uint64_t offset = 0; offset < file_info.size;) {
        uint64_t amount = file_info.size - offset;
        if (amount > sizeof(chunk)) {
            amount = sizeof(chunk);
        }
        if (!read_chunk(path, offset, chunk, amount)) {
            return 1;
        }

        for (uint64_t i = 0; i < amount; i++) {
            uint8_t value = chunk[i];
            if (value == '\n') {
                if (line_matches) {
                    line[line_length] = '\0';
                    print_u64(line_number);
                    print(": ");
                    println(line);
                    line_found = 1;
                }
                line_number++;
                line_length = 0;
                matched = 0;
                line_matches = 0;
                continue;
            }

            if (line_length < DOGEUTIL_LINE_SIZE) {
                line[line_length++] = (char)value;
            }

            while (matched > 0 && value != (uint8_t)needle[matched]) {
                matched = prefix[matched - 1];
            }
            if (value == (uint8_t)needle[matched]) {
                matched++;
                if (matched == needle_length) {
                    line_matches = 1;
                    matched = prefix[matched - 1];
                }
            }
        }
        offset += amount;
    }

    if (line_length != 0 || line_matches) {
        if (line_matches) {
            line[line_length] = '\0';
            print_u64(line_number);
            print(": ");
            println(line);
            line_found = 1;
        }
    }

    if (!line_found) {
        println("dogeutil: no matches");
        return 1;
    }
    return 0;
}

static int command_wc(const char *path)
{
    dogec_stat_t file_info = {0};
    if ((int64_t)stat(path, &file_info) < 0 || file_info.exists == 0 ||
        file_info.is_dir != 0) {
        println("dogeutil: cannot read that file");
        return 1;
    }

    uint8_t chunk[DOGEUTIL_IO_SIZE];
    uint64_t lines = 0;
    uint64_t words = 0;
    uint64_t offset = 0;
    int in_word = 0;

    while (offset < file_info.size) {
        uint64_t amount = file_info.size - offset;
        if (amount > sizeof(chunk)) {
            amount = sizeof(chunk);
        }
        if (!read_chunk(path, offset, chunk, amount)) {
            return 1;
        }
        for (uint64_t i = 0; i < amount; i++) {
            if (chunk[i] == '\n') {
                lines++;
            }
            if (is_space(chunk[i])) {
                in_word = 0;
            } else if (!in_word) {
                words++;
                in_word = 1;
            }
        }
        offset += amount;
    }

    print("lines ");
    print_u64(lines);
    print("  words ");
    print_u64(words);
    print("  bytes ");
    print_u64(file_info.size);
    print("  ");
    println(path);
    return 0;
}

static int command_find(const char *fragment)
{
    if (fragment[0] == '\0') {
        println("dogeutil: provide a name fragment");
        return 2;
    }

    char names[DOGEUTIL_LIST_CAPACITY][256];
    int64_t count = (int64_t)list_dir(names, DOGEUTIL_LIST_CAPACITY);
    if (count < 0) {
        println("dogeutil: unable to list current directory");
        return 1;
    }

    int found = 0;
    for (int64_t i = 0; i < count; i++) {
        if (contains_text(names[i], fragment)) {
            println(names[i]);
            found = 1;
        }
    }
    if (!found) {
        println("dogeutil: no matching names");
        return 1;
    }
    return 0;
}

static int command_stat(const char *path)
{
    dogec_stat_t file_info = {0};
    if ((int64_t)stat(path, &file_info) < 0 || file_info.exists == 0) {
        println("dogeutil: path not found");
        return 1;
    }

    print(file_info.is_dir != 0 ? "directory " : "file ");
    print_u64(file_info.size);
    print(" bytes  ");
    println(path);
    return 0;
}

static int command_exists(const char *path)
{
    if (file_exists(path) == 1) {
        println("yes");
        return 0;
    }
    println("no");
    return 1;
}

static int command_ls(void)
{
    char names[DOGEUTIL_LIST_CAPACITY][256];
    int64_t count = (int64_t)list_dir(names, DOGEUTIL_LIST_CAPACITY);
    if (count < 0) {
        println("dogeutil: unable to list current directory");
        return 1;
    }
    for (int64_t i = 0; i < count; i++) {
        println(names[i]);
    }
    return 0;
}

static int parse_byte_count(const char *text, uint64_t *result)
{
    if (text[0] == '\0') {
        return 0;
    }
    uint64_t value = 0;
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        uint64_t digit = (uint64_t)(text[i] - '0');
        if (value > (32 - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
    }
    if (value == 0 || value > 32) {
        return 0;
    }
    *result = value;
    return 1;
}

static int command_random(const char *count_text)
{
    uint64_t count = 8;
    if (count_text != NULL && !parse_byte_count(count_text, &count)) {
        println("dogeutil: random byte-count must be between 1 and 32");
        return 2;
    }

    uint8_t bytes[32];
    if ((int64_t)doge_random_bytes(bytes, count) < 0) {
        println("dogeutil: random source failed");
        return 1;
    }
    static const char digits[] = "0123456789abcdef";
    char output[65];
    for (uint64_t i = 0; i < count; i++) {
        output[i * 2] = digits[bytes[i] >> 4];
        output[i * 2 + 1] = digits[bytes[i] & 0x0fU];
    }
    output[count * 2] = '\0';
    println(output);
    return 0;
}

static int command_uname(void)
{
    windoge_utsname_t info;
    if ((int64_t)doge_uname(&info) < 0) {
        println("dogeutil: uname request failed");
        return 1;
    }
    println(info.sysname);
    println(info.nodename);
    println(info.release);
    println(info.version);
    println(info.machine);
    println(info.domainname);
    return 0;
}

static int command_which(const char *name)
{
    char path[128];
    static const char *directories[] = {
        "/apps/", "/system/"
    };
    for (size_t directory = 0;
         directory < sizeof(directories) / sizeof(directories[0]);
         directory++) {
        str_strcpy(path, directories[directory]);
        size_t position = str_strlen(path);
        size_t name_length = str_strlen(name);
        if (name_length + position + 5 > sizeof(path)) {
            continue;
        }
        for (size_t i = 0; i < name_length; i++) {
            path[position + i] = name[i];
        }
        position += name_length;
        str_strcpy(path + position, ".elf");
        if (file_exists(path) == 1) {
            println(path);
            return 0;
        }
    }
    println("dogeutil: program not found");
    return 1;
}

void _start(int argc, char **argv)
{
    if (argc < 1 || argv == NULL || argv[0] == NULL ||
        text_equal(argv[0], "--help")) {
        sys_exit((uint64_t)usage());
        return;
    }

    int status = 2;
    if (text_equal(argv[0], "grep") && argc == 3 &&
        argv[1] != NULL && argv[2] != NULL) {
        status = command_grep(argv[1], argv[2]);
    } else if (text_equal(argv[0], "find") && argc == 2 &&
               argv[1] != NULL) {
        status = command_find(argv[1]);
    } else if (text_equal(argv[0], "wc") && argc == 2 &&
               argv[1] != NULL) {
        status = command_wc(argv[1]);
    } else if (text_equal(argv[0], "stat") && argc == 2 &&
               argv[1] != NULL) {
        status = command_stat(argv[1]);
    } else if (text_equal(argv[0], "exists") && argc == 2 &&
               argv[1] != NULL) {
        status = command_exists(argv[1]);
    } else if (text_equal(argv[0], "ls") && argc == 1) {
        status = command_ls();
    } else if (text_equal(argv[0], "random") && argc <= 2) {
        status = command_random(argc == 2 ? argv[1] : NULL);
    } else if (text_equal(argv[0], "time") && argc == 1) {
        uint64_t epoch = 0;
        if ((int64_t)get_time(&epoch) < 0) {
            println("dogeutil: time request failed");
            status = 1;
        } else {
            print_u64(epoch);
            println("");
            status = 0;
        }
    } else if (text_equal(argv[0], "user") && argc == 1) {
        char username[64];
        if ((int64_t)doge_get_user_name(username, sizeof(username)) < 0) {
            println("dogeutil: user request failed");
            status = 1;
        } else {
            println(username);
            status = 0;
        }
    } else if (text_equal(argv[0], "cwd") && argc == 1) {
        char path[256];
        if ((int64_t)get_cwd(path, sizeof(path)) < 0) {
            println("dogeutil: working directory request failed");
            status = 1;
        } else {
            println(path);
            status = 0;
        }
    } else if (text_equal(argv[0], "uname") && argc == 1) {
        status = command_uname();
    } else if (text_equal(argv[0], "which") && argc == 2 &&
               argv[1] != NULL) {
        status = command_which(argv[1]);
    } else {
        usage();
    }

    sys_exit((uint64_t)status);
}
