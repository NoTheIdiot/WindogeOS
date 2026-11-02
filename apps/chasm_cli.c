#include "../library/dogeio.h"
#include "../library/stddef.h"
#include "../library/stdint.h"
#include "asm_x64.h"

#define CHASM_SOURCE_CAPACITY 65536
#define CHASM_MAX_INSTRUCTIONS 16384
#define CHASM_MAX_LABELS 512
#define CHASM_MAX_REFERENCES 4096
#define CHASM_LABEL_SIZE 32
#define CHASM_PATH_SIZE 128
#define CHASM_ELF_LOAD_OFFSET 4096
#define CHASM_ELF_ENTRY_ADDRESS 0x400000
#define CHASM_ELF_LOAD_FLAGS 7

typedef struct __attribute__((packed)) {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_entry_size;
    uint16_t program_header_count;
    uint16_t section_header_entry_size;
    uint16_t section_header_count;
    uint16_t section_name_index;
} chasm_elf64_header_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
} chasm_elf64_program_header_t;

_Static_assert(sizeof(chasm_elf64_header_t) == 64,
               "ELF64 header layout must match the ELF ABI");
_Static_assert(sizeof(chasm_elf64_program_header_t) == 56,
               "ELF64 program header layout must match the ELF ABI");

typedef struct {
    const char *name;
    x64Operand operand;
} register_entry_t;

typedef struct {
    char name[CHASM_LABEL_SIZE];
    uint32_t instruction;
    int64_t value;
    uint8_t is_constant;
} label_entry_t;

typedef struct {
    char name[CHASM_LABEL_SIZE];
    uint32_t instruction;
    uint8_t operand;
    uint8_t kind;
} reference_entry_t;

enum {
    REFERENCE_BRANCH,
    REFERENCE_MEMORY,
    REFERENCE_CONSTANT
};

static char source_buffer[CHASM_SOURCE_CAPACITY + 1]
    __attribute__((section(".bss.chasm_cli")));
static x64Ins instructions[CHASM_MAX_INSTRUCTIONS]
    __attribute__((section(".bss.chasm_cli")));
static uint8_t data_records[CHASM_MAX_INSTRUCTIONS]
    __attribute__((section(".bss.chasm_cli")));
static uint8_t is_data_record[CHASM_MAX_INSTRUCTIONS]
    __attribute__((section(".bss.chasm_cli")));
static label_entry_t labels[CHASM_MAX_LABELS]
    __attribute__((section(".bss.chasm_cli")));
static reference_entry_t references[CHASM_MAX_REFERENCES]
    __attribute__((section(".bss.chasm_cli")));
static uint32_t output_offsets[CHASM_MAX_INSTRUCTIONS + 1]
    __attribute__((section(".bss.chasm_cli")));
static uint8_t elf_padding[
    CHASM_ELF_LOAD_OFFSET - sizeof(chasm_elf64_header_t) -
    sizeof(chasm_elf64_program_header_t)]
    __attribute__((section(".bss.chasm_cli")));
static char input_path[CHASM_PATH_SIZE]
    __attribute__((section(".bss.chasm_cli")));
static char output_path[CHASM_PATH_SIZE]
    __attribute__((section(".bss.chasm_cli")));

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
    {"dec", DEC}, {"lea", LEA}, {"push", PUSH}, {"pop", POP},
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

static int find_label_entry(const char *name, uint32_t label_count) {
    for (uint32_t i = 0; i < label_count; i++) {
        if (text_equal(name, labels[i].name)) {
            return (int)i;
        }
    }
    return -1;
}

static int add_reference(const char *name, uint32_t instruction,
                         uint8_t operand, uint8_t kind,
                         uint32_t *reference_count) {
    if (!valid_identifier(name) || text_length(name) >= CHASM_LABEL_SIZE ||
        *reference_count >= CHASM_MAX_REFERENCES) {
        return -1;
    }
    reference_entry_t *reference = &references[*reference_count];
    size_t length = text_length(name);
    for (size_t i = 0; i <= length; i++) {
        reference->name[i] = name[i];
    }
    lowercase(reference->name);
    reference->instruction = instruction;
    reference->operand = operand;
    reference->kind = kind;
    (*reference_count)++;
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

static int parse_operand(char *text, x64Operand *operand,
                         uint32_t instruction_index, uint8_t operand_index,
                         uint32_t *reference_count) {
    text = skip_space(text);
    trim_end(text);
    if (parse_register(text, operand) == 0) {
        return 0;
    }

    int64_t value;
    if (parse_number(text, &value) == 0) {
        operand->type = IMM8 | IMM16 | IMM32 | IMM64;
        operand->value = value;
        return 0;
    }

    uint64_t memory_type = M8 | M16 | M32 | M64 | M128 | M256 | M512;
    char *open_bracket = text;
    char *qualifier_end = text;
    while (*qualifier_end != '\0' && *qualifier_end != ' ' &&
           *qualifier_end != '\t') {
        qualifier_end++;
    }
    char qualifier_delimiter = *qualifier_end;
    *qualifier_end = '\0';
    int has_qualifier = 1;
    if (text_equal(text, "byte")) {
        memory_type = M8;
    } else if (text_equal(text, "word")) {
        memory_type = M16;
    } else if (text_equal(text, "dword")) {
        memory_type = M32;
    } else if (text_equal(text, "qword")) {
        memory_type = M64;
    } else if (text_equal(text, "xmmword")) {
        memory_type = M128;
    } else {
        has_qualifier = 0;
    }
    *qualifier_end = qualifier_delimiter;
    if (has_qualifier) {
        open_bracket = skip_space(qualifier_end);
        if (*open_bracket != '[') {
            char *ptr = open_bracket;
            while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t') {
                ptr++;
            }
            char ptr_delimiter = *ptr;
            *ptr = '\0';
            int has_ptr = text_equal(open_bracket, "ptr");
            *ptr = ptr_delimiter;
            if (!has_ptr || ptr_delimiter == '\0') {
                return -1;
            }
            open_bracket = skip_space(ptr);
        }
    }
    if (*open_bracket == '[') {
        char *close_bracket = open_bracket + 1;
        while (*close_bracket != '\0' && *close_bracket != ']') {
            close_bracket++;
        }
        if (*close_bracket != ']') {
            return -1;
        }
        *close_bracket = '\0';
        char *symbol = skip_space(open_bracket + 1);
        if (symbol[0] == 'r' && symbol[1] == 'e' && symbol[2] == 'l' &&
            (symbol[3] == ' ' || symbol[3] == '\t')) {
            symbol = skip_space(symbol + 3);
        }
        trim_end(symbol);
        if (!valid_identifier(symbol) ||
            add_reference(symbol, instruction_index, operand_index,
                          REFERENCE_MEMORY, reference_count) != 0 ||
            *skip_space(close_bracket + 1) != '\0') {
            return -1;
        }
        operand->type = memory_type;
        operand->value = x64mem($riprel, 0);
        return 0;
    }

    if (valid_identifier(text) &&
        add_reference(text, instruction_index, operand_index,
                      REFERENCE_CONSTANT, reference_count) == 0) {
        operand->type = NONE;
        operand->value = 0;
        return 0;
    }
    return -1;
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
    labels[*label_count].value = 0;
    labels[*label_count].is_constant = 0;
    (*label_count)++;
    return 0;
}

static int add_constant(char *name, int64_t value, uint32_t *label_count) {
    if (add_label(name, 0, label_count) != 0) {
        return -1;
    }
    labels[*label_count - 1].value = value;
    labels[*label_count - 1].is_constant = 1;
    return 0;
}

static int is_data_directive(const char *text, uint32_t *width) {
    if (text_equal(text, "db") || text_equal(text, ".byte")) {
        *width = 1;
    } else if (text_equal(text, "dw") || text_equal(text, ".word")) {
        *width = 2;
    } else if (text_equal(text, "dd") || text_equal(text, ".long")) {
        *width = 4;
    } else if (text_equal(text, "dq") || text_equal(text, ".quad")) {
        *width = 8;
    } else {
        return 0;
    }
    return 1;
}

static int append_data_byte(uint8_t value, uint32_t *record_count,
                            uint32_t *data_size) {
    if (*record_count >= CHASM_MAX_INSTRUCTIONS) {
        return -1;
    }
    x64Ins *instruction = &instructions[*record_count];
    instruction->op = INT3;
    for (size_t i = 0; i < 4; i++) {
        instruction->params[i].type = NONE;
        instruction->params[i].value = 0;
    }
    data_records[*record_count] = value;
    is_data_record[*record_count] = 1;
    (*record_count)++;
    (*data_size)++;
    return 0;
}

static int parse_quoted_data(char **text, uint32_t *record_count,
                             uint32_t *data_size) {
    char quote = **text;
    (*text)++;
    while (**text != '\0' && **text != quote) {
        uint8_t value = (uint8_t)**text;
        if (**text == '\\') {
            (*text)++;
            if (**text == '\0') {
                return -1;
            }
            switch (**text) {
                case 'n': value = '\n'; break;
                case 'r': value = '\r'; break;
                case 't': value = '\t'; break;
                case '0': value = '\0'; break;
                case '\\': value = '\\'; break;
                case '\'': value = '\''; break;
                case '"': value = '"'; break;
                default: return -1;
            }
        }
        if (append_data_byte(value, record_count, data_size) != 0) {
            return -1;
        }
        (*text)++;
    }
    if (**text != quote) {
        return -1;
    }
    (*text)++;
    return 0;
}

static int parse_data_directive(char *text, uint32_t width,
                                uint32_t *record_count,
                                uint32_t *data_size) {
    text = skip_space(text);
    if (*text == '\0') {
        return -1;
    }

    while (*text != '\0') {
        text = skip_space(text);
        if (*text == '"' || *text == '\'') {
            if (width != 1 ||
                parse_quoted_data(&text, record_count, data_size) != 0) {
                return -1;
            }
        } else {
            char *end = text;
            while (*end != '\0' && *end != ',') {
                end++;
            }
            char delimiter = *end;
            *end = '\0';
            trim_end(text);
            int64_t value;
            if (parse_number(text, &value) != 0) {
                return -1;
            }
            uint32_t bit_count = width * 8U;
            int invalid_range = width < 8 &&
                (value < -(INT64_C(1) << (bit_count - 1U)) ||
                 (value >= 0 &&
                  (uint64_t)value >= (UINT64_C(1) << bit_count)));
            if (invalid_range) {
                return -1;
            }
            uint64_t bits = (uint64_t)value;
            for (uint32_t byte = 0; byte < width; byte++) {
                if (append_data_byte((uint8_t)(bits >> (byte * 8U)),
                                     record_count, data_size) != 0) {
                    return -1;
                }
            }
            text = end + (delimiter == ',' ? 1 : 0);
        }

        text = skip_space(text);
        if (*text == '\0') {
            break;
        }
        if (*text != ',') {
            return -1;
        }
        text++;
        if (*skip_space(text) == '\0') {
            return -1;
        }
    }
    return 0;
}

static int parse_source(char *source, uint32_t *instruction_count,
                        uint32_t *source_instruction_count,
                        uint32_t *data_size, uint32_t *entry_instruction,
                        uint32_t *error_line) {
    uint32_t label_count = 0;
    uint32_t reference_count = 0;
    *instruction_count = 0;
    *source_instruction_count = 0;
    *data_size = 0;
    *entry_instruction = UINT32_MAX;
    uint32_t line_number = 0;

    for (uint32_t i = 0; i < CHASM_MAX_INSTRUCTIONS; i++) {
        instructions[i].op = END_ASM;
        data_records[i] = 0;
        is_data_record[i] = 0;
        for (size_t j = 0; j < 4; j++) {
            instructions[i].params[j].type = NONE;
            instructions[i].params[j].value = 0;
        }
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
        char quote = '\0';
        int escaped = 0;
        while (*comment != '\0') {
            if (escaped) {
                escaped = 0;
            } else if (quote != '\0' && *comment == '\\') {
                escaped = 1;
            } else if (quote != '\0' && *comment == quote) {
                quote = '\0';
            } else if (quote == '\0' &&
                       (*comment == '"' || *comment == '\'')) {
                quote = *comment;
            } else if (quote == '\0' &&
                       (*comment == ';' || *comment == '#')) {
                break;
            }
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

        uint32_t data_width;
        if (is_data_directive(mnemonic, &data_width)) {
            if (parse_data_directive(line, data_width, instruction_count,
                                     data_size) != 0) {
                *error_line = line_number;
                return -1;
            }
            line = next_line;
            continue;
        }

        if (valid_identifier(mnemonic) && *line != '\0') {
            char *directive = line;
            while (*directive != '\0' && *directive != ' ' &&
                   *directive != '\t') {
                directive++;
            }
            char delimiter = *directive;
            *directive = '\0';
            lowercase(line);
            uint32_t declaration_width;
            int data_declaration = is_data_directive(line, &declaration_width);
            int constant_declaration = text_equal(line, "equ");
            *directive = delimiter;
            if (data_declaration || constant_declaration) {
                char *arguments = skip_space(delimiter == '\0'
                                                 ? directive
                                                 : directive + 1);
                if (data_declaration) {
                    if (add_label(mnemonic, *instruction_count,
                                  &label_count) != 0 ||
                        parse_data_directive(arguments, declaration_width,
                                             instruction_count,
                                             data_size) != 0) {
                        *error_line = line_number;
                        return -1;
                    }
                } else {
                    int64_t value;
                    if (parse_number(arguments, &value) != 0 ||
                        add_constant(mnemonic, value, &label_count) != 0) {
                        *error_line = line_number;
                        return -1;
                    }
                }
                line = next_line;
                continue;
            }
        }

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
            if (add_reference(line, *instruction_count, 0,
                              REFERENCE_BRANCH, &reference_count) != 0) {
                *error_line = line_number;
                return -1;
            }
            instruction->params[0].type = REL32;
            (*source_instruction_count)++;
            (*instruction_count)++;
            line = next_line;
            continue;
        }

        if (*line != '\0') {
            uint8_t operand_index = 0;
            char *operand_text = line;
            while (true) {
                char *comma = operand_text;
                while (*comma != '\0' && *comma != ',') {
                    comma++;
                }
                char has_comma = *comma;
                *comma = '\0';
                if (operand_index >= 4 ||
                    *skip_space(operand_text) == '\0' ||
                    parse_operand(operand_text,
                                  &instruction->params[operand_index],
                                  *instruction_count, operand_index,
                                  &reference_count) != 0) {
                    *error_line = line_number;
                    return -1;
                }
                operand_index++;
                if (has_comma == '\0') {
                    break;
                }
                operand_text = comma + 1;
                if (*skip_space(operand_text) == '\0') {
                    *error_line = line_number;
                    return -1;
                }
            }
        }
        if (*entry_instruction == UINT32_MAX) {
            *entry_instruction = *instruction_count;
        }
        (*source_instruction_count)++;
        (*instruction_count)++;
        line = next_line;
    }

    if (*source_instruction_count == 0) {
        *error_line = line_number == 0 ? 1 : line_number;
        return -1;
    }

    int start_label = find_label_entry("_start", label_count);
    if (start_label >= 0) {
        if (labels[start_label].is_constant ||
            labels[start_label].instruction >= *instruction_count ||
            is_data_record[labels[start_label].instruction]) {
            *error_line = 0;
            return -1;
        }
        *entry_instruction = labels[start_label].instruction;
    }

    for (uint32_t i = 0; i < reference_count; i++) {
        reference_entry_t *reference = &references[i];
        int symbol_index = find_label_entry(reference->name, label_count);
        if (symbol_index < 0) {
            *error_line = 0;
            return -1;
        }
        label_entry_t *symbol = &labels[symbol_index];
        x64Operand *operand =
            &instructions[reference->instruction].params[reference->operand];
        if (reference->kind == REFERENCE_CONSTANT) {
            if (!symbol->is_constant) {
                *error_line = 0;
                return -1;
            }
            operand->type = IMM8 | IMM16 | IMM32 | IMM64;
            operand->value = symbol->value;
            continue;
        }
        if (symbol->is_constant) {
            *error_line = 0;
            return -1;
        }
        int64_t relative =
            (int64_t)symbol->instruction - reference->instruction;
        if (reference->kind == REFERENCE_BRANCH) {
            operand->value = relative;
        } else {
            operand->value = x64mem($riprel, 0) | (uint32_t)relative;
        }
    }
    return 0;
}

static int has_elf_extension(const char *path) {
    size_t length = text_length(path);
    return length >= 4 &&
           path[length - 4] == '.' &&
           path[length - 3] == 'e' &&
           path[length - 2] == 'l' &&
           path[length - 1] == 'f';
}

static void initialize_elf_headers(
    chasm_elf64_header_t *header,
    chasm_elf64_program_header_t *program_header, uint32_t code_size,
    uint64_t entry_address) {
    for (size_t i = 0; i < sizeof(header->ident); i++) {
        header->ident[i] = 0;
    }
    header->ident[0] = 0x7f;
    header->ident[1] = 'E';
    header->ident[2] = 'L';
    header->ident[3] = 'F';
    header->ident[4] = 2;
    header->ident[5] = 1;
    header->ident[6] = 1;
    header->type = 2;
    header->machine = 62;
    header->version = 1;
    header->entry = entry_address;
    header->program_header_offset = sizeof(*header);
    header->section_header_offset = 0;
    header->flags = 0;
    header->header_size = sizeof(*header);
    header->program_header_entry_size = sizeof(*program_header);
    header->program_header_count = 1;
    header->section_header_entry_size = 0;
    header->section_header_count = 0;
    header->section_name_index = 0;

    program_header->type = 1;
    program_header->flags = CHASM_ELF_LOAD_FLAGS;
    program_header->offset = CHASM_ELF_LOAD_OFFSET;
    program_header->virtual_address = CHASM_ELF_ENTRY_ADDRESS;
    program_header->physical_address = CHASM_ELF_ENTRY_ADDRESS;
    program_header->file_size = code_size;
    program_header->memory_size = code_size;
    program_header->alignment = CHASM_ELF_LOAD_OFFSET;
}

static int write_elf_file(const char *output_name, const uint8_t *code,
                          uint32_t code_size, uint64_t entry_address) {
    chasm_elf64_header_t header;
    chasm_elf64_program_header_t program_header;
    initialize_elf_headers(&header, &program_header, code_size,
                           entry_address);

    if (file_exists(output_name)) {
        println("chasm: output already exists; remove it first");
        return -1;
    }
    if (create_file(output_name) != 0) {
        println("chasm: unable to create ELF output");
        return -1;
    }
    if ((int64_t)append_file(output_name, &header, sizeof(header)) < 0 ||
        (int64_t)append_file(output_name, &program_header,
                             sizeof(program_header)) < 0 ||
        (int64_t)append_file(output_name, elf_padding,
                             sizeof(elf_padding)) < 0 ||
        (int64_t)append_file(output_name, code, code_size) < 0) {
        delete_file(output_name);
        println("chasm: unable to write ELF output");
        return -1;
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
    if (!has_elf_extension(output_name)) {
        println("chasm: output filename must end in .elf");
        return 1;
    }

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
    uint32_t source_instruction_count;
    uint32_t data_size;
    uint32_t entry_instruction;
    uint32_t error_line;
    if (parse_source(source_buffer, &instruction_count,
                     &source_instruction_count, &data_size,
                     &entry_instruction,
                     &error_line) != 0) {
        if (error_line != 0) {
            print("chasm: invalid or unsupported instruction at line ");
            print_u32(error_line);
            println("");
        } else {
            println("chasm: undefined symbol or invalid symbol reference");
        }
        return 1;
    }

    output_offsets[0] = 0;
    uint8_t encoded_instruction[15];
    for (uint32_t i = 0; i < instruction_count; i++) {
        uint32_t encoded_size = x64emit(&instructions[i],
                                        encoded_instruction);
        if (encoded_size == 0 ||
            output_offsets[i] > UINT32_MAX - encoded_size) {
            println("chasm: instruction encoding failed");
            return 1;
        }
        output_offsets[i + 1] = output_offsets[i] + encoded_size;
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
    if (output_offsets[instruction_count] != output_size) {
        println("chasm: internal output size mismatch");
        x64free(machine_code);
        return 1;
    }
    for (uint32_t i = 0; i < instruction_count; i++) {
        if (is_data_record[i]) {
            machine_code[output_offsets[i]] = data_records[i];
        }
    }

    uint64_t entry_address =
        CHASM_ELF_ENTRY_ADDRESS + output_offsets[entry_instruction];
    if (write_elf_file(output_name, machine_code, output_size,
                       entry_address) != 0) {
        x64free(machine_code);
        return 1;
    }

    x64free(machine_code);
    print("chasm: assembled ");
    print_u32(source_instruction_count);
    print(" instructions and ");
    print_u32(data_size);
    print(" data bytes into ");
    println(output_name);
    return 0;
}

void _start(int argc, char **argv) {
    int status = 0;
    if (argc == 1 && text_equal(argv[0], "--help")) {
        println("Usage: chasm <source.asm> <output.elf>");
        println("Run chasm without arguments to enter the paths interactively.");
        println("Outputs an x86-64 ELF executable.");
        println("Supports instructions, labels, DB/DW/DD/DQ, and EQU constants.");
        println("Use [label] or [rel label] for RIP-relative data references.");
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
        println("Usage: chasm <source.asm> <output.elf>");
        status = 2;
    } else {
        if ((int64_t)input("Input .asm file: ", input_path,
                           sizeof(input_path)) < 0 ||
            (int64_t)input("Output .elf file: ", output_path,
                           sizeof(output_path)) < 0) {
            println("chasm: input failed");
            status = 1;
        } else {
            status = assemble_file(input_path, output_path);
        }
    }
    exit((uint64_t)status);
}
