#include "../dogec/stdio.h"
#include "../dogec/dogeio.h"
#include "../dogec/string.h"

#define MAX_LINES      1024
#define MAX_LINE_LEN   256
#define VIEW_LINES     20

static char lines[MAX_LINES][MAX_LINE_LEN];
static char fname[64];

static int nlines = 1;

static int cur_line = 0;
static int cur_col  = 0;

static int top_line = 0;
static int left_col = 0;

static int dirty = 0;

static char clipboard[MAX_LINE_LEN];

static int str_len(const char *s)
{
    int n = 0;
    while (s && s[n])
        n++;
    return n;
}

static void copy_str(char *dst, const char *src, int cap)
{
    int i = 0;

    if (!dst || cap <= 0)
        return;

    if (src)
    {
        while (i < cap - 1 && src[i])
        {
            dst[i] = src[i];
            i++;
        }
    }

    dst[i] = 0;
}

static int str_eq(const char *a, const char *b)
{
    int i = 0;

    while (a[i] && b[i])
    {
        if (a[i] != b[i])
            return 0;

        i++;
    }

    return a[i] == b[i];
}

static int prompt(const char *msg, char *out, int cap)
{
    int pos = 0;

    out[0] = 0;

    printf("\n%s", msg);

    while (1)
    {
        int k = getkey();

        if (k < 0)
        {
            wait(10);
            continue;
        }

        if (k == 27)
        {
            putchar('\n');
            return 0;
        }

        if (k == '\r' || k == '\n')
            break;

        if ((k == 8 || k == 127) && pos > 0)
        {
            pos--;
            out[pos] = 0;
            printf("\b \b");
        }
        else if (k >= 32 && k < 127 && pos < cap - 1)
        {
            out[pos++] = k;
            out[pos] = 0;
            putchar(k);
        }
    }

    putchar('\n');

    return 1;
}

static void adjust_scroll(void)
{
    if (cur_line < top_line)
        top_line = cur_line;

    if (cur_line >= top_line + VIEW_LINES)
        top_line = cur_line - VIEW_LINES + 1;

    if (cur_col < left_col)
        left_col = cur_col;

    if (cur_col >= left_col + 70)
        left_col = cur_col - 69;
}

static void insert_char(char c)
{
    char *line = lines[cur_line];

    int len = str_len(line);

    if (len >= MAX_LINE_LEN - 1)
        return;

    for (int i = len; i >= cur_col; i--)
        line[i + 1] = line[i];

    line[cur_col] = c;

    cur_col++;

    dirty = 1;
}

static void insert_newline(void)
{
    if (nlines >= MAX_LINES)
        return;

    char right[MAX_LINE_LEN];

    copy_str(right,
             lines[cur_line] + cur_col,
             MAX_LINE_LEN);

    lines[cur_line][cur_col] = 0;

    for (int i = nlines; i > cur_line + 1; i--)
        copy_str(lines[i], lines[i - 1], MAX_LINE_LEN);

    copy_str(lines[cur_line + 1], right, MAX_LINE_LEN);

    nlines++;

    cur_line++;
    cur_col = 0;

    dirty = 1;
}

static void backspace(void)
{
    char *line = lines[cur_line];

    if (cur_col > 0)
    {
        int len = str_len(line);

        for (int i = cur_col - 1; i < len; i++)
            line[i] = line[i + 1];

        cur_col--;
        dirty = 1;
        return;
    }

    if (cur_line == 0)
        return;

    int prev_len = str_len(lines[cur_line - 1]);
    int cur_len  = str_len(line);

    if (prev_len + cur_len >= MAX_LINE_LEN)
        return;

    copy_str(
        lines[cur_line - 1] + prev_len,
        line,
        MAX_LINE_LEN - prev_len
    );

    for (int i = cur_line; i < nlines - 1; i++)
        copy_str(lines[i], lines[i + 1], MAX_LINE_LEN);

    nlines--;

    cur_line--;
    cur_col = prev_len;

    dirty = 1;
}

static void delete_forward(void)
{
    char *line = lines[cur_line];

    int len = str_len(line);

    if (cur_col < len)
    {
        for (int i = cur_col; i < len; i++)
            line[i] = line[i + 1];

        dirty = 1;
        return;
    }

    if (cur_line >= nlines - 1)
        return;

    int next_len = str_len(lines[cur_line + 1]);

    if (len + next_len >= MAX_LINE_LEN)
        return;

    copy_str(
        line + len,
        lines[cur_line + 1],
        MAX_LINE_LEN - len
    );

    for (int i = cur_line + 1; i < nlines - 1; i++)
        copy_str(lines[i], lines[i + 1], MAX_LINE_LEN);

    nlines--;

    dirty = 1;
}

static void move_left(void)
{
    if (cur_col > 0)
    {
        cur_col--;
        return;
    }

    if (cur_line > 0)
    {
        cur_line--;
        cur_col = str_len(lines[cur_line]);
    }
}

static void move_right(void)
{
    int len = str_len(lines[cur_line]);

    if (cur_col < len)
    {
        cur_col++;
        return;
    }

    if (cur_line < nlines - 1)
    {
        cur_line++;
        cur_col = 0;
    }
}

static void move_up(void)
{
    if (cur_line > 0)
    {
        cur_line--;

        int len = str_len(lines[cur_line]);

        if (cur_col > len)
            cur_col = len;
    }
}

static void move_down(void)
{
    if (cur_line < nlines - 1)
    {
        cur_line++;

        int len = str_len(lines[cur_line]);

        if (cur_col > len)
            cur_col = len;
    }
}

static void save_file(void)
{
    if (!fname[0])
        return;

    FILE *fp = fopen(fname, "w");

    if (!fp)
        return;

    for (int i = 0; i < nlines; i++)
    {
        fwrite(lines[i], 1, str_len(lines[i]), fp);
        fputc('\n', fp);
    }

    fclose(fp);

    dirty = 0;
}

static int find_line(const char *s)
{
    for (int i = 0; i < nlines; i++)
    {
        if (strstr(lines[i], s))
            return i;
    }

    return -1;
}

static void draw_editor(void)
{
    clear();

    printf(
        "[%s%s] Ln:%d Col:%d\n",
        fname[0] ? fname : "Untitled",
        dirty ? "*" : "",
        cur_line + 1,
        cur_col + 1
    );

    printf("------------------------------------------------------------\n");

    int end = top_line + VIEW_LINES;

    if (end > nlines)
        end = nlines;

    for (int y = top_line; y < end; y++)
    {
        printf("%c ",
               y == cur_line ? '>' : ' ');

        int len = str_len(lines[y]);

        for (int x = left_col; x <= len; x++)
        {
            if (y == cur_line && x == cur_col)
                putchar('|');

            if (x < len)
                putchar(lines[y][x]);
        }

        putchar('\n');
    }

    printf("------------------------------------------------------------\n");
    printf("^S Save  ^Q Quit  ^F Find  ^G Goto  ^K CutLine  ^U Paste\n");
}

void _start(int argc, char **argv)
{
    fname[0] = 0;

    for (int i = 0; i < MAX_LINES; i++)
        lines[i][0] = 0;

    clipboard[0] = 0;

    if (argc > 1)
        copy_str(fname, argv[1], sizeof(fname));
        if (fname[0]) {
            FILE *fp = fopen(fname, "r");

            if (fp)
            {
                int line = 0;
                int pos = 0;
                int ch;

                lines[0][0] = 0;

                while ((ch = fgetc(fp)) >= 0)
                {
                    if (ch == '\r')
                        continue;

                    if (ch == '\n')
                    {
                        lines[line][pos] = 0;

                        if (line < MAX_LINES - 1)
                        {
                            line++;
                            pos = 0;
                            lines[line][0] = 0;
                        }

                        continue;
                    }

                    if (pos < MAX_LINE_LEN - 1)
                    {
                        lines[line][pos++] = (char)ch;
                        lines[line][pos] = 0;
                    }
                }

                nlines = line + 1;

                if (nlines < 1)
                    nlines = 1;

                fclose(fp);
            }
        }

    while (1)
    {
        adjust_scroll();
        draw_editor();

        int k;

        do {
            k = getkey();

            if (k < 0)
                wait(10);

        } while (k < 0);

        switch (k)
        {
            case KEY_LEFT:  move_left();  break;
            case KEY_RIGHT: move_right(); break;
            case KEY_UP:    move_up();    break;
            case KEY_DOWN:  move_down();  break;

            case '\r':
            case '\n':
                insert_newline();
                break;

            case 8:
            case 127:
                backspace();
                break;

            case 19: /* Ctrl+S */
                save_file();
                break;

            case 17: /* Ctrl+Q */
                if (!dirty)
                    goto quit;
                break;

            case 6: /* Ctrl+F */
            {
                char search[64];

                if (prompt("Find: ", search, sizeof(search)))
                {
                    int line = find_line(search);

                    if (line >= 0)
                    {
                        cur_line = line;
                        cur_col = 0;
                    }
                }
                break;
            }

            case 7: /* Ctrl+G */
            {
                char num[16];

                if (prompt("Goto line: ", num, sizeof(num)))
                {
                    int line = atoi(num);

                    if (line > 0 && line <= nlines)
                    {
                        cur_line = line - 1;
                        cur_col = 0;
                    }
                }
                break;
            }

            default:
                if (k >= 32 && k < 127)
                    insert_char((char)k);
                break;
        }
    }

quit:
    clear();
    exit();
}