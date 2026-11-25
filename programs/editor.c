#include "../dogec/stdio.h"
#include "../dogec/string.h"
#include "../dogec/dogeio.h"

#define EDITOR_MAX_FILE 65536
#define SCREEN_ROWS 24
#define SCREEN_COLS 80

typedef struct
{
    char *buf;
    uint64_t len;
    uint64_t cap;

    uint32_t cx;
    uint32_t cy;

    uint32_t rowoff;

    char filename[256];

    bool modified;
} editor_t;

static editor_t E;

static void strcopy(char *dst, const char *src)
{
    while (*src)
        *dst++ = *src++;

    *dst = 0;
}

static uint64_t strlength(const char *s)
{
    uint64_t n = 0;

    while (s[n])
        n++;

    return n;
}

static uint32_t editor_total_lines(void)
{
    uint32_t lines = 1;

    for (uint64_t i = 0; i < E.len; i++)
    {
        if (E.buf[i] == '\n')
            lines++;
    }

    return lines;
}

static uint64_t editor_row_start(uint32_t row)
{
    uint32_t cur = 0;

    for (uint64_t i = 0; i < E.len; i++)
    {
        if (cur == row)
            return i;

        if (E.buf[i] == '\n')
            cur++;
    }

    return E.len;
}

static uint64_t editor_cursor_offset(void)
{
    uint64_t pos = editor_row_start(E.cy);

    uint32_t x = 0;

    while (pos < E.len &&
           E.buf[pos] != '\n' &&
           x < E.cx)
    {
        pos++;
        x++;
    }

    return pos;
}

static void editor_insert_char(char c)
{
    uint64_t pos = editor_cursor_offset();

    if (E.len + 2 >= E.cap)
        return;

    for (uint64_t i = E.len; i > pos; i--)
        E.buf[i] = E.buf[i - 1];

    E.buf[pos] = c;

    E.len++;
    E.buf[E.len] = 0;

    if (c == '\n')
    {
        E.cy++;
        E.cx = 0;
    }
    else
    {
        E.cx++;
    }

    E.modified = true;
}

static void editor_backspace(void)
{
    uint64_t pos = editor_cursor_offset();

    if (pos == 0)
        return;

    pos--;

    char removed = E.buf[pos];

    for (uint64_t i = pos; i < E.len - 1; i++)
        E.buf[i] = E.buf[i + 1];

    E.len--;

    if (removed == '\n')
    {
        if (E.cy)
            E.cy--;
    }
    else
    {
        if (E.cx)
            E.cx--;
    }

    E.modified = true;
}

static void editor_load(const char *filename)
{
    FILE *f = fopen(filename, "r");

    if (!f)
        return;

    E.len = fread(E.buf, 1, E.cap - 1, f);

    fclose(f);

    E.buf[E.len] = 0;

    strcopy(E.filename, filename);

    E.modified = false;
}

static void editor_save(void)
{
    if (!E.filename[0])
        return;

    FILE *f = fopen(E.filename, "w");

    if (!f)
        return;

    fwrite(E.buf, 1, E.len, f);

    fclose(f);

    E.modified = false;
}

static void editor_scroll(void)
{
    if (E.cy < E.rowoff)
        E.rowoff = E.cy;

    if (E.cy >= E.rowoff + (SCREEN_ROWS - 4))
        E.rowoff = E.cy - (SCREEN_ROWS - 5);
}

static void editor_draw_rows(void)
{
    uint32_t line = 0;
    uint32_t screen_line = 0;

    printf("\033[H");

    while (line < E.rowoff)
    {
        uint64_t start = editor_row_start(line);

        while (start < E.len && E.buf[start] != '\n')
            start++;

        line++;
    }

    while (screen_line < (SCREEN_ROWS - 4))
    {
        uint64_t start = editor_row_start(line);

        if (start >= E.len)
        {
            printf("~\n");
            screen_line++;
            line++;
            continue;
        }

        printf("%4u ", line + 1);

        uint32_t col = 0;

        while (start < E.len &&
               E.buf[start] != '\n' &&
               col < SCREEN_COLS - 6)
        {
            putchar(E.buf[start]);
            start++;
            col++;
        }

        putchar('\n');

        screen_line++;
        line++;
    }
}

static void editor_draw_status(void)
{
    printf("-------------------------------------------------------------------------------\n");

    printf("%s %s | Ln %u Col %u\n",
           E.filename[0] ? E.filename : "[No Name]",
           E.modified ? "*" : "",
           E.cy + 1,
           E.cx + 1);

    printf("Ctrl-S Save | Ctrl-Q Quit | WASD Move\n");
}

static void editor_refresh_screen(void)
{
    editor_scroll();

    clear();

    editor_draw_rows();

    editor_draw_status();
}

static void editor_move_left(void)
{
    if (E.cx)
        E.cx--;
}

static void editor_move_right(void)
{
    E.cx++;
}

static void editor_move_up(void)
{
    if (E.cy)
        E.cy--;
}

static void editor_move_down(void)
{
    if (E.cy + 1 < editor_total_lines())
        E.cy++;
}

static void editor_process_key(int key)
{
    switch (key)
    {
        case 19:
            editor_save();
            break;

        case 17:
            clear();
            exit();
            break;

        case 8:
            editor_backspace();
            break;

        case '\r':
        case '\n':
            editor_insert_char('\n');
            break;

        case 'w':
        case 'W':
            editor_move_up();
            break;

        case 's':
        case 'S':
            editor_move_down();
            break;

        case 'a':
        case 'A':
            editor_move_left();
            break;

        case 'd':
        case 'D':
            editor_move_right();
            break;

        default:
            if (key >= 32 && key <= 126)
                editor_insert_char((char)key);
            break;
    }
}

static void editor_loop(void)
{
    while (1)
    {
        editor_refresh_screen();

        int key = getkey();

        editor_process_key(key);
    }
}

int main(int argc, char **argv)
{
    E.cap = EDITOR_MAX_FILE;

    E.buf = sbrk(EDITOR_MAX_FILE);

    if (!E.buf)
    {
        printf("failed to allocate memory\n");
        return 1;
    }

    E.len = 0;
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.modified = false;
    E.filename[0] = 0;

    if (argc > 1)
    {
        editor_load(argv[1]);
    }

    editor_loop();

    return 0;
}