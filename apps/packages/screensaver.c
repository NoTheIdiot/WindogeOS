#include "../library/dogeio.h"
#include "../library/bool.h"

#define COLS 160
#define ROWS 50

static int heads[COLS];
static int lengths[COLS];

static uint32_t rand32(void)
{
    uint32_t value = 0U;
    doge_random_bytes(&value, sizeof(value));
    return value;
}

void _start(void)
{
    bool restore_bar = is_bar_on();
    disable_bar();
    clear();
    background_color(COLOR_BLACK);

    for (int i = 0; i < COLS; i++) {
        heads[i] = -(int)(rand32() % 60U);
        lengths[i] = 8 + (int)(rand32() % 15U);
    }

    while (true) {
        if ((uint16_t)get_key_delay(15000U) != KEY_UNKNOWN) {
            break;
        }

        for (int x = 0; x < COLS; x++) {
            heads[x]++;

            int y = heads[x];

            if ((y >= 0) && (y < ROWS)) {
                char ch[2];
                ch[0] = (char)('!' + (int)(rand32() % 94U));
                ch[1] = '\0';
                print_at(ch, (uint32_t)x, (uint32_t)y, COLOR_GREEN);
            }

            int tail = y - lengths[x];
            if ((tail >= 0) && (tail < ROWS)) {
                print_at(" ", (uint32_t)x, (uint32_t)tail, COLOR_BLACK);
            }

            if (heads[x] >= (ROWS + lengths[x])) {
                heads[x] = -(int)(rand32() % 40U);
                lengths[x] = 8 + (int)(rand32() % 15U);
            }
        }
    }

    clear();
    if (restore_bar) {
        enable_bar();
    }
    sys_exit(0);
}