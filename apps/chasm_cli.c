#include "../library/dogeio.h"
#include "../library/stddef.h"
#include "../library/stdint.h"
#include "asm_x64.h"

#define CHASM_SOURCE_CAPACITY 16384
#define CHASM_MAX_INSTRUCTIONS 1024
#define CHASM_MAX_LABELS 128
#define CHASM_LABEL_SIZE 32
#define CHASM_PATH_SIZE 128

typedef struct {
    const char *name;
    x64Operand operand;
} register_entry_t;

typedef struct {
    char name[CHASM_LABEL_SIZE];
    uint32_t instruction;
} label_entry_t;

static char source_buffer[CHASM_SOURCE_CAPACITY + 1]
    __attribute__((section(".data.chasm_cli"))) = {1};
static x64Ins instructions[CHASM_MAX_INSTRUCTIONS]
    __attribute__((section(".data.chasm_cli"))) = {{.op = NOP}};
static char branch_labels[CHASM_MAX_INSTRUCTIONS][CHASM_LABEL_SIZE]
    __attribute__((section(".data.chasm_cli"))) = {{1}};
static label_entry_t labels[CHASM_MAX_LABELS]
    __attribute__((section(".data.chasm_cli"))) = {{.name = {1}}};
static char input_path[CHASM_PATH_SIZE]
    __attribute__((section(".data.chasm_cli"))) = {1};
static char output_path[CHASM_PATH_SIZE]
    __attribute__((section(".data.chasm_cli"))) = {1};

static const register_entry_t registers[] = {
    {"rax", {R64 | RAX, 0}}, {"rcx", {R64, 1}},
    {"rdx", {R64, 2}}, {"rbx", {R64, 3}},
    {"rsp", {R64, 4}}, {"rbp", {R64, 5}},
    {"rsi", {R64, 6}}, {"rdi", {R64, 7}},
    {"r8", {R64, 8}}, {"r9", {R64, 9}},
    {"r10", {R64, 10}}, {"r11", {R64, 11}},
    {"r12", {R64, 12}}, {"r13", {R64, 13}},
    {"r14", {R64, 14}}, {"r15", {R64, 15}},
    {"eax", {R32 | EAX, $eax}}, {"ecx", {R32, $ecx}},
    {"edx", {R32, $edx}}, {"ebx", {R32, $ebx}},
    {"esp", {R32, $esp}}, {"ebp", {R32, $ebp}},
    {"esi", {R32, $esi}}, {"edi", {R32, $edi}},
    {"r8d", {R32, $r8d}}, {"r9d", {R32, $r9d}},
    {"r10d", {R32, $r10d}}, {"r11d", {R32, $r11d}},
    {"r12d", {R32, $r12d}}, {"r13d", {R32, $r13d}},
    {"r14d", {R32, $r14d}}, {"r15d", {R32, $r15d}},
    {"ax", {R16 | AX, 0}}, {"cx", {R16, 1}},
    {"dx", {R16 | DX, 2}}, {"bx", {R16, 3}},
    {"sp", {R16, 4}}, {"bp", {R16, 5}},
    {"si", {R16, 6}}, {"di", {R16, 7}},
    {"r8w", {R16, 8}}, {"r9w", {R16, 9}},
    {"r10w", {R16, 10}}, {"r11w", {R16, 11}},
    {"r12w", {R16, 12}}, {"r13w", {R16, 13}},
    {"r14w", {R16, 14}}, {"r15w", {R16, 15}},
    {"al", {R8 | AL, 0}}, {"cl", {R8 | CL, 1}},
    {"dl", {R8, 2}}, {"bl", {R8, 3}},
    {"spl", {R8, 4}}, {"bpl", {R8, 5}},
    {"sil", {R8, 6}}, {"dil", {R8, 7}},
    {"r8b", {R8, 8}}, {"r9b", {R8, 9}},
    {"r10b", {R8, 10}}, {"r11b", {R8, 11}},
    {"r12b", {R8, 12}}, {"r13b", {R8, 13}},
    {"r14b", {R8, 14}}, {"r15b", {R8, 15}},
};

typedef struct {
    const char *name;
    x64Op op;
} operation_entry_t;

static const operation_entry_t operations[] = {
    {"nop", NOP}, {"ret", RET}, {"syscall", SYSCALL},
    {"int3", INT3}, {"leave", LEAVE}, {"mov", MOV},
    {"add", ADD}, {"sub", SUB}, {"xor", XOR},
    {"and", AND}, {"or", OR}, {"cmp", CMP},
    {"test", TEST}, {"imul", IMUL}, {"inc", INC},
    {"dec", DEC}, {"push", PUSH}, {"pop", POP},
    {"neg", NEG}, {"not", NOT}, {"int", INT},
    {"call", CALL}, {"jmp", JMP}, {"je", JE},
    {"jne", JNE}, {"jz", JZ}, {"jnz", JNZ},
    {"ja", JA}, {"jae", JAE}, {"jb", JB},
    {"jbe", JBE}, {"jg", JG}, {"jge", JGE},
    {"jl", JL}, {"jle", JLE}, {"jo", JO},
    {"jno", JNO}, {"js", JS}, {"jns", JNS},
    {"jp", JP}, {"jnp", JNP}, {"jc", JC},
    {"jnc", JNC},
};

static int text_equal(const char *left, const char *right) {
    size_t i = 0;
    while (left[i] != '\0' && right[i] != '\0' &&
           left[i] == right[i]) {
        i++;
    }
    return left[i] == '\0' && right[i] == '\0';
}

static size_t text_length(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static char *skip_space(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r') {
        text++;
    }
    return text;
}

static void trim_end(char *text) {
    size_t length = text_length(text);
    while (length > 0 &&
           (text[length - 1] == ' ' || text[length - 1] == '\t' ||
            text[length - 1] == '\r')) {
        text[--length] = '\0';
    }
}

static int valid_identifier(const char *text) {
    if (!((*text >= 'a' && *text <= 'z') ||
          (*text >= 'A' && *text <= 'Z') || *text == '_')) {
        return 0;
    }
    for (size_t i = 1; text[i] != '\0'; i++) {
        if (!((text[i] >= 'a' && text[i] <= 'z') ||
              (text[i] >= 'A' && text[i] <= 'Z') ||
              (text[i] >= '0' && text[i] <= '9') || text[i] == '_')) {
            return 0;
        }
    }
    return 1;
}

static void lowercase(char *text) {
    while (*text != '\0') {
        if (*text >= 'A' && *text <= 'Z') {
            *text = (char)(*text - 'A' + 'a');
        }
        text++;
    }
}

static int parse_number(const char *text, int64_t *result) {
    int negative = 0;
    uint32_t radix = 10;
    uint64_t value = 0;
    size_t index = 0;

    if (text[index] == '+' || text[index] == '-') {
        negative = text[index] == '-';
        index++;
    }
    if (text[index] == '0' &&
        (text[index + 1] == 'x' || text[index + 1] == 'X')) {
        radix = 16;
        index += 2;
    }
    if (text[index] == '\0') {
        return -1;
    }
    for (; text[index] != '\0'; index++) {
        uint32_t digit;
        if (text[index] >= '0' && text[index] <= '9') {
            digit = (uint32_t)(text[index] - '0');
        } else if (radix == 16 && text[index] >= 'a' && text[index] <= 'f') {
            digit = (uint32_t)(text[index] - 'a') + 10U;
        } else if (radix == 16 && text[index] >= 'A' && text[index] <= 'F') {
            digit = (uint32_t)(text[index] - 'A') + 10U;
        } else {
            return -1;
        }
        if (digit >= radix || value > (UINT64_MAX - digit) / radix) {
            return -1;
        }
        value = value * radix + digit;
    }
    if ((negative && value > (uint64_t)INT64_MAX + 1U) ||
        (!negative && value > (uint64_t)INT64_MAX)) {
        return -1;
    }
    *result = negative ? (int64_t)(0U - value) : (int64_t)value;
    return 0;
}

static int parse_register(const char *text, x64Operand *operand) {
    for (size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); i++) {
        if (text_equal(text, registers[i].name)) {
            *operand = registers[i].operand;
            return 0;
        }
    }
    return -1;
}

static int parse_operand(char *text, x64Operand *operand) {
    text = skip_space(text);
    trim_end(text);
    if (parse_register(text, operand) == 0) {
        return 0;
    }

    int64_t value;
    if (parse_number(text, &value) != 0) {
        return -1;
    }
    operand->type = IMM8 | IMM16 | IMM32 | IMM64;
    operand->value = value;
    return 0;
}

static int find_operation(const char *text, x64Op *operation) {
    for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
        if (text_equal(text, operations[i].name)) {
            *operation = operations[i].op;
            return 0;
        }
    }
    return -1;
}

static int is_branch(x64Op operation) {
    switch (operation) {
        case CALL:
        case JMP:
        case JE:
        case JNE:
        case JZ:
        case JNZ:
        case JA:
        case JAE:
        case JB:
        case JBE:
        case JG:
        case JGE:
        case JL:
        case JLE:
        case JO:
        case JNO:
        case JS:
        case JNS:
        case JP:
        case JNP:
        case JC:
        case JNC:
            return 1;
        default:
            return 0;
    }
}

static int add_label(char *name, uint32_t instruction_count,
                     uint32_t *label_count) {
    lowercase(name);
    if (!valid_identifier(name) || text_length(name) >= CHASM_LABEL_SIZE ||
        *label_count >= CHASM_MAX_LABELS) {
        return -1;
    }
    for (uint32_t i = 0; i < *label_count; i++) {
        if (text_equal(name, labels[i].name)) {
            return -1;
        }
    }
    size_t length = text_length(name);
    for (size_t i = 0; i <= length; i++) {
        labels[*label_count].name[i] = name[i];
    }
    labels[*label_count].instruction = instruction_count;
    (*label_count)++;
    return 0;
}

static int find_label(const char *name, uint32_t label_count,
                      uint32_t *instruction) {
    for (uint32_t i = 0; i < label_count; i++) {
        if (text_equal(name, labels[i].name)) {
            *instruction = labels[i].instruction;
            return 0;
        }
    }
    return -1;
}

static int parse_source(char *source, uint32_t *instruction_count,
                        uint32_t *error_line) {
    uint32_t label_count = 0;
    *instruction_count = 0;
    uint32_t line_number = 0;

    for (uint32_t i = 0; i < CHASM_MAX_INSTRUCTIONS; i++) {
        instructions[i].op = END_ASM;
        for (size_t j = 0; j < 4; j++) {
            instructions[i].params[j].type = NONE;
            instructions[i].params[j].value = 0;
        }
        branch_labels[i][0] = '\0';
    }

    char *line = source;
    while (*line != '\0') {
        line_number++;
        char *next_line = line;
        while (*next_line != '\0' && *next_line != '\n') {
            next_line++;
        }
        if (*next_line == '\n') {
            *next_line = '\0';
            next_line++;
        }
        char *comment = line;
        while (*comment != '\0' && *comment != ';' && *comment != '#') {
            comment++;
        }
        *comment = '\0';
        line = skip_space(line);
        trim_end(line);
        if (*line == '\0') {
            line = next_line;
            continue;
        }

        char *colon = line;
        while (*colon != '\0' && *colon != ':' && *colon != ' ' &&
               *colon != '\t') {
            colon++;
        }
        if (*colon == ':') {
            *colon = '\0';
            if (add_label(line, *instruction_count, &label_count) != 0) {
                *error_line = line_number;
                return -1;
            }
            line = skip_space(colon + 1);
            if (*line == '\0') {
                line = next_line;
                continue;
            }
        }

        char *mnemonic = line;
        while (*line != '\0' && *line != ' ' && *line != '\t') {
            line++;
        }
        if (*line != '\0') {
            *line++ = '\0';
        }
        lowercase(mnemonic);
        line = skip_space(line);

        if (text_equal(mnemonic, "bits") ||
            text_equal(mnemonic, "section") ||
            text_equal(mnemonic, "global")) {
            if (text_equal(mnemonic, "bits") && !text_equal(line, "64")) {
                *error_line = line_number;
                return -1;
            }
            line = next_line;
            continue;
        }
        if (*instruction_count >= CHASM_MAX_INSTRUCTIONS) {
            *error_line = line_number;
            return -1;
        }

        x64Op operation;
        if (find_operation(mnemonic, &operation) != 0) {
            *error_line = line_number;
            return -1;
        }
        x64Ins *instruction = &instructions[*instruction_count];
        instruction->op = operation;

        if (is_branch(operation)) {
            if (*line == '\0' || text_length(line) >= CHASM_LABEL_SIZE) {
                *error_line = line_number;
                return -1;
            }
            lowercase(line);
            if (!valid_identifier(line)) {
                *error_line = line_number;
                return -1;
            }
            size_t length = text_length(line);
            for (size_t i = 0; i <= length; i++) {
                branch_labels[*instruction_count][i] = line[i];
            }
            instruction->params[0].type = REL8 | REL32;
            (*instruction_count)++;
            line = next_line;
            continue;
        }

        if (*line != '\0') {
            char *comma = line;
            while (*comma != '\0' && *comma != ',') {
                comma++;
            }
            if (*comma == '\0') {
                if (parse_operand(line, &instruction->params[0]) != 0) {
                    *error_line = line_number;
                    return -1;
                }
            } else {
                *comma = '\0';
                if (parse_operand(line, &instruction->params[0]) != 0 ||
                    parse_operand(comma + 1, &instruction->params[1]) != 0 ||
                    text_equal(skip_space(comma + 1), "")) {
                    *error_line = line_number;
                    return -1;
                }
                char *second = skip_space(comma + 1);
                trim_end(second);
                for (char *c = second; *c != '\0'; c++) {
                    if (*c == ',') {
                        *error_line = line_number;
                        return -1;
                    }
                }
            }
        }
        (*instruction_count)++;
        line = next_line;
    }

    if (*instruction_count == 0) {
        *error_line = line_number == 0 ? 1 : line_number;
        return -1;
    }

    for (uint32_t i = 0; i < *instruction_count; i++) {
        if (branch_labels[i][0] == '\0') {
            continue;
        }
        uint32_t target;
        if (find_label(branch_labels[i], label_count, &target) != 0) {
            *error_line = 0;
            return -1;
        }
        instructions[i].params[0].value = (int64_t)target - (int64_t)i;
    }
    return 0;
}

static void print_u32(uint32_t value) {
    char digits[11];
    size_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0);
    while (count > 0) {
        char character[2] = {digits[--count], '\0'};
        print(character);
    }
}

static int copy_path(char *destination, const char *source) {
    size_t length = text_length(source);
    if (length == 0 || length >= CHASM_PATH_SIZE) {
        return -1;
    }
    for (size_t i = 0; i <= length; i++) {
        destination[i] = source[i];
    }
    return 0;
}

static int assemble_file(const char *source_name, const char *output_name) {
    int64_t source_size = (int64_t)read_file(
        source_name, source_buffer, CHASM_SOURCE_CAPACITY);
    if (source_size < 0) {
        println("chasm: unable to read input file");
        return 1;
    }
    if ((uint64_t)source_size == CHASM_SOURCE_CAPACITY) {
        println("chasm: input file is too large");
        return 1;
    }
    source_buffer[source_size] = '\0';

    uint32_t instruction_count;
    uint32_t error_line;
    if (parse_source(source_buffer, &instruction_count, &error_line) != 0) {
        if (error_line != 0) {
            print("chasm: invalid or unsupported instruction at line ");
            print_u32(error_line);
            println("");
        } else {
            println("chasm: branch target label was not found");
        }
        return 1;
    }

    uint32_t output_size;
    uint8_t *machine_code = x64as(instructions, instruction_count, &output_size);
    if (machine_code == NULL) {
        x64ErrorType error_type;
        char *error_message = x64error(&error_type);
        (void)error_type;
        print("chasm: assembly failed");
        if (error_message != NULL) {
            print(": ");
            print(error_message);
        }
        println("");
        return 1;
    }

    if (file_exists(output_name)) {
        println("chasm: output already exists; remove it first");
        x64free(machine_code);
        return 1;
    }
    if (create_file(output_name) != 0) {
        println("chasm: unable to create output file");
        x64free(machine_code);
        return 1;
    }
    if ((int64_t)append_file(output_name, machine_code, output_size) < 0) {
        delete_file(output_name);
        println("chasm: unable to write output file");
        x64free(machine_code);
        return 1;
    }

    x64free(machine_code);
    print("chasm: assembled ");
    print_u32(instruction_count);
    print(" instructions into ");
    println(output_name);
    return 0;
}

void _start(int argc, char **argv) {
    int status = 0;
    if (argc == 1 && text_equal(argv[0], "--help")) {
        println("Usage: chasm <source.asm> <output.bin>");
        println("Run chasm without arguments to enter the paths interactively.");
        status = 0;
    } else if (argc == 2) {
        if (copy_path(input_path, argv[0]) != 0 ||
            copy_path(output_path, argv[1]) != 0) {
            println("chasm: paths must be 1 to 127 characters");
            status = 1;
        } else {
            status = assemble_file(input_path, output_path);
        }
    } else if (argc != 0) {
        println("Usage: chasm <source.asm> <output.bin>");
        status = 2;
    } else {
        if ((int64_t)input("Input .asm file: ", input_path,
                           sizeof(input_path)) < 0 ||
            (int64_t)input("Output .bin file: ", output_path,
                           sizeof(output_path)) < 0) {
            println("chasm: input failed");
            status = 1;
        } else {
            status = assemble_file(input_path, output_path);
        }
    }
    sys_exit((uint64_t)status);
}
