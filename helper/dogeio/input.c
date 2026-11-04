#include <stdint.h>
#include <bool.h>
#include <stddef.h>
#include <basicutil.h>
#include <dogeio.h>
#include <string.h>
#include <time.h>
#include <boot/kernel.h>
#include <boot/scheduler.h>

const char map_lower[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,   0,
   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
   'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

const char map_upper[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
    0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,
   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
   'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

extern char text_grid[TERMINAL_ROWS * TERMINAL_COLS];

static bool global_shift_pressed = false;
static bool global_ctrl_pressed  = false;
static bool irq_ctrl_pressed = false;

#define SERIAL_PORT 0x2F8

static volatile uint8_t serial_mouse_packet[3];
static volatile uint8_t serial_mouse_index;

static volatile uint8_t ps2_buttons;
static volatile uint8_t serial_buttons;

static bool status_bar_enabled = true;

#define KEYBOARD_SCANCODE_BUFFER_SIZE 256

static volatile uint8_t keyboard_scancodes[KEYBOARD_SCANCODE_BUFFER_SIZE];
static volatile uint8_t keyboard_scancode_head;
static volatile uint8_t keyboard_scancode_tail;

static uint64_t interrupt_save_disable(void)
{
    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0; cli"
                      : "=r"(rflags) : : "memory");
    return rflags;
}

static void interrupt_restore(uint64_t rflags)
{
    if ((rflags & (1ULL << 9)) != 0) {
        sti();
    }
}

#define MOUSE_PACKET_SIZE 3

static volatile uint8_t mouse_packet[MOUSE_PACKET_SIZE];
static volatile uint8_t mouse_packet_index;
static volatile int32_t mouse_delta_x;
static volatile int32_t mouse_delta_y;
static volatile uint8_t mouse_buttons;

static bool ps2_wait_input_empty(void)
{
    for (uint32_t i = 0; i < 100000U; i++) {
        if ((ports_inb(0x64) & 0x02U) == 0) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    return false;
}

static bool ps2_wait_output_full(void)
{
    for (uint32_t i = 0; i < 100000U; i++) {
        if ((ports_inb(0x64) & 0x01U) != 0) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    return false;
}

static bool ps2_send_mouse_command(uint8_t command)
{
    if (!ps2_wait_input_empty()) {
        return false;
    }
    ports_outb(0x64, 0xD4);
    if (!ps2_wait_input_empty()) {
        return false;
    }
    ports_outb(0x60, command);
    if (!ps2_wait_output_full()) {
        return false;
    }
    return ports_inb(0x60) == 0xFA;
}

static void serial_mouse_parse(uint8_t byte)
{
    if (byte & 0x40U) {
        serial_mouse_index = 0;
    }

    if (serial_mouse_index >= 3) {
        serial_mouse_index = 0;
    }

    serial_mouse_packet[serial_mouse_index++] = byte;

    if (serial_mouse_index != 3) {
        return;
    }

    serial_mouse_index = 0;

    uint8_t b1 = serial_mouse_packet[0];
    uint8_t b2 = serial_mouse_packet[1];
    uint8_t b3 = serial_mouse_packet[2];

    int dx =
        ((int)((b1 & 0x03U) << 6)) |
        ((int)(b2 & 0x3FU));

    int dy =
        ((int)((b1 & 0x0CU) << 4)) |
        ((int)(b3 & 0x3FU));

    if (dx & 0x80) {
        dx -= 256;
    }

    if (dy & 0x80) {
        dy -= 256;
    }

    mouse_delta_x += dx;
    mouse_delta_y -= dy;

    serial_buttons = 0;

    if (b1 & 0x20U) {
        serial_buttons |= 0x01;
    }

    if (b1 & 0x10U) {
        serial_buttons |= 0x02;
    }

    mouse_buttons = ps2_buttons | serial_buttons;
}

void dogeio_serial_mouse_irq(void)
{
    while (serial_received()) {
        serial_mouse_parse(serial_read());
    }
}

bool dogeio_ps2_mouse_init(void)
{
    if (!ps2_wait_input_empty()) {
        return false;
    }
    ports_outb(0x64, 0xA8);

    if (!ps2_wait_input_empty()) {
        return false;
    }
    ports_outb(0x64, 0x20);
    if (!ps2_wait_output_full()) {
        return false;
    }
    uint8_t controller_config = ports_inb(0x60);

    if (!ps2_wait_input_empty()) {
        return false;
    }
    ports_outb(0x64, 0x60);
    if (!ps2_wait_input_empty()) {
        return false;
    }
    ports_outb(0x60, (uint8_t)((controller_config | 0x02U) & ~0x20U));

    if (!ps2_send_mouse_command(0xF6) ||
        !ps2_send_mouse_command(0xF4)) {
        return false;
    }

    mouse_packet_index = 0;
    mouse_delta_x = 0;
    mouse_delta_y = 0;

    mouse_buttons = 0;
    ps2_buttons = 0;
    serial_buttons = 0;

    serial_mouse_index = 0;

    return true;
}

bool dogeio_serial_mouse_init(void)
{
    serial_mouse_index = 0;

    ports_outb(SERIAL_PORT + 4, 0x0B);
    ports_outb(SERIAL_PORT + 1, 0x01);

    return true;
}

void dogeio_keyboard_irq(void)
{
    uint8_t scancode = ports_inb(0x60);
    if (scancode == 0x1D) {
        irq_ctrl_pressed = true;
    } else if (scancode == 0x9D) {
        irq_ctrl_pressed = false;
    } else if (scancode == 0x0F && irq_ctrl_pressed) {
        sys_workspace_request_switch();
        return;
    }

    uint8_t next_head = (uint8_t)(keyboard_scancode_head + 1U);

    if (next_head != keyboard_scancode_tail) {
        keyboard_scancodes[keyboard_scancode_head] = scancode;
        keyboard_scancode_head = next_head;
    }
}

void dogeio_mouse_irq(void)
{
    uint8_t byte = ports_inb(0x60);
    uint8_t index = mouse_packet_index;

    if (index == 0 && (byte & 0x08U) == 0) {
        return;
    }

    mouse_packet[index] = byte;
    index++;
    if (index < MOUSE_PACKET_SIZE) {
        mouse_packet_index = index;
        return;
    }

    mouse_packet_index = 0;
    uint8_t flags = mouse_packet[0];
    if ((flags & 0xC0U) != 0) {
        return;
    }

    mouse_delta_x += (int8_t)mouse_packet[1];
    mouse_delta_y -= (int8_t)mouse_packet[2];
    mouse_buttons = (uint8_t)(flags & 0x07U);
}

bool dogeio_mouse_read(int32_t *delta_x, int32_t *delta_y,
                       uint8_t *buttons)
{
    if (delta_x == NULL || delta_y == NULL || buttons == NULL) {
        return false;
    }

    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(rflags) : : "memory");
    *delta_x = mouse_delta_x;
    *delta_y = mouse_delta_y;
    *buttons = mouse_buttons;
    mouse_delta_x = 0;
    mouse_delta_y = 0;
    if ((rflags & (1ULL << 9)) != 0) {
        sti();
    }
    return true;
}

static uint16_t dogeio_decode_key(uint8_t code)
{
    static bool escaped = false;

    switch (code) {
        case 0xE0:
            escaped = true;
            return KEY_UNKNOWN;

        case 0x2A:
        case 0x36:
            global_shift_pressed = true;
            return KEY_UNKNOWN;

        case 0xAA:
        case 0xB6:
            global_shift_pressed = false;
            return KEY_UNKNOWN;

        case 0x1D:
            global_ctrl_pressed = true;
            return KEY_UNKNOWN;

        case 0x9D:
            global_ctrl_pressed = false;
            return KEY_UNKNOWN;
    }

    if (code & 0x80) {
        escaped = false;
        return KEY_UNKNOWN;
    }

    if (escaped) {
        escaped = false;

        switch (code) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
        }

        return KEY_UNKNOWN;
    }

    switch (code) {
        case 0x1C: return KEY_ENTER;
        case 0x0E: return KEY_BACKSPACE;
        case 0x0F: 
            if (global_ctrl_pressed) {
                return KEY_CTRL_TAB;
            }
            return KEY_TAB;

        case 0x48: return KEY_UP;
        case 0x50: return KEY_DOWN;
        case 0x4B: return KEY_LEFT;
        case 0x4D: return KEY_RIGHT;
    }

    if (code < 58) {
        char c = global_shift_pressed ? map_upper[code] : map_lower[code];

        if (c != 0) {
            if (global_ctrl_pressed) {
                if (c == 'o') return KEY_CTRL_O;
                if (c == 'n') return KEY_CTRL_N;

                if (c >= 'a' && c <= 'z')
                    return (uint16_t)(c - 'a' + 1);

                if (c >= 'A' && c <= 'Z')
                    return (uint16_t)(c - 'A' + 1);
            }

            return (uint16_t)c;
        }
    }

    return KEY_UNKNOWN;
}

uint16_t dogeio_get_key_delay(uint64_t loops) {
    uint64_t rflags = interrupt_save_disable();
    if (scheduler_current_workspace() != (uint32_t)current_terminal_id) {
        interrupt_restore(rflags);
        return KEY_UNKNOWN;
    }

    while (loops > 0 && keyboard_scancode_tail != keyboard_scancode_head) {
        uint8_t scancode = keyboard_scancodes[keyboard_scancode_tail];
        keyboard_scancode_tail = (uint8_t)(keyboard_scancode_tail + 1U);
        loops--;

        uint16_t key = dogeio_decode_key(scancode);
        if (key != KEY_UNKNOWN) {
            interrupt_restore(rflags);
            return key;
        }
    }
    interrupt_restore(rflags);
    return KEY_UNKNOWN;
}

void dogeio_discard_pending_keys(void) {
    uint64_t rflags = interrupt_save_disable();
    keyboard_scancode_tail = keyboard_scancode_head;
    global_shift_pressed = false;
    global_ctrl_pressed = false;
    interrupt_restore(rflags);
}

uint16_t dogeio_get_key(void) {
    uint32_t workspace_generation = sys_workspace_generation();
    while (true) {
        if (workspace_generation != sys_workspace_generation()) {
            return KEY_CTRL_TAB;
        }

        if (scheduler_current_workspace() != (uint32_t)current_terminal_id) {
            __asm__ volatile ("sti; hlt" ::: "memory");
            continue;
        }

        if (status_bar_enabled && !is_kernel_dead &&
            pit_take_status_bar_refresh()) {
            menubar_draw();
        }

        uint64_t rflags = interrupt_save_disable();
        if (scheduler_current_workspace() !=
            (uint32_t)current_terminal_id) {
            interrupt_restore(rflags);
            continue;
        }
        if (keyboard_scancode_tail != keyboard_scancode_head) {
            uint8_t scancode = keyboard_scancodes[keyboard_scancode_tail];
            keyboard_scancode_tail = (uint8_t)(keyboard_scancode_tail + 1U);
            interrupt_restore(rflags);

            uint16_t key = dogeio_decode_key(scancode);
            if (key != KEY_UNKNOWN) {
                if (status_bar_enabled && !is_kernel_dead &&
                    pit_take_status_bar_refresh()) {
                    menubar_draw();
                }
                return key;
            }
            continue;
        }

        __asm__ volatile ("sti; hlt" ::: "memory");
    }
}


void dogeio_input(const char *prompt, char *buffer, size_t max_size) {
    if (prompt != NULL) {
        dogeio_print(prompt);
    }

    size_t len = 0;
    size_t pos = 0;
    uint32_t workspace_generation = sys_workspace_generation();
    buffer[0] = '\0';

    dogeio_cursor_show();

    while (true) {
        uint16_t key = dogeio_get_key();

        if (workspace_generation != sys_workspace_generation()) {
            dogeio_println("");
            dogeio_println("[Input cancelled]");
            len = 0;
            pos = 0;
            buffer[0] = '\0';
            workspace_generation = sys_workspace_generation();
            if (prompt != NULL) {
                dogeio_print(prompt);
            }
            dogeio_cursor_show();
            continue;
        }

        if (key == KEY_CTRL_TAB) {
            dogeio_println("");
            dogeio_println("[Input cancelled]");
            sys_switch_terminal();
            workspace_generation = sys_workspace_generation();
            len = 0;
            pos = 0;
            buffer[0] = '\0';
            if (prompt != NULL) {
                dogeio_print(prompt);
            }
            dogeio_cursor_show();
            continue;
        }

        if (key == KEY_ENTER) {
            dogeio_print("\n");
            break;
        }

        if (key == KEY_LEFT) {
            if (pos > 0) {
                dogeio_cursor_hide();
                pos--;
                if (cursor_x > 0) {
                    cursor_x--;
                } else if (cursor_y > 1) {
                    cursor_y--;
                    cursor_x = TERMINAL_COLS - 1;
                }
                dogeio_cursor_show();
            }
            continue;
        }

        if (key == KEY_TAB || key == 9 || key == 0x000F || key == 0x000D) {
            if (len + 8 < max_size) {
                dogeio_cursor_hide();
                
                for (size_t i = len + 8; i > pos + 7; i--) {
                    buffer[i] = buffer[i - 8];
                }
                
                for (size_t i = 0; i < 8; i++) {
                    buffer[pos + i] = ' ';
                }
                
                len += 8;
                buffer[len] = '\0';

                
                uint32_t saved_x = cursor_x;
                uint32_t saved_y = cursor_y;

                for (size_t i = pos; i < len; i++) {
                    char ch = buffer[i];
                    text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ch;
                    dogeio_putchar(ch, cursor_x, cursor_y);
                    cursor_x++;
                    if (cursor_x >= TERMINAL_COLS) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }
                
                cursor_x = saved_x;
                cursor_y = saved_y;
                for (size_t i = 0; i < 8; i++) {
                    cursor_x++;
                    if (cursor_x >= TERMINAL_COLS) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }
                pos += 8;

                dogeio_cursor_show();
            }
            continue;
        }

        if (key == KEY_RIGHT) {
            if (pos < len) {
                dogeio_cursor_hide();
                pos++;
                cursor_x++;
                if (cursor_x >= TERMINAL_COLS) {
                    cursor_x = 0;
                    cursor_y++;
                }
                dogeio_cursor_show();
            }
            continue;
        }

        if (key == KEY_BACKSPACE) {
            if (pos > 0) {
                dogeio_cursor_hide();
                
                for (size_t i = pos - 1; i < len; i++) {
                    buffer[i] = buffer[i + 1];
                }
                len--;
                pos--;
                buffer[len] = '\0';

                if (cursor_x > 0) {
                    cursor_x--;
                } else if (cursor_y > 1) {
                    cursor_y--;
                    cursor_x = TERMINAL_COLS - 1;
                }

                uint32_t saved_x = cursor_x;
                uint32_t saved_y = cursor_y;

                for (size_t i = pos; i <= len; i++) {
                    char c = (i < len) ? buffer[i] : ' ';
                    text_grid[cursor_y * TERMINAL_COLS + cursor_x] = c;
                    dogeio_putchar(c, cursor_x, cursor_y);
                    cursor_x++;
                    if (cursor_x >= TERMINAL_COLS) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }

                cursor_x = saved_x;
                cursor_y = saved_y;
                dogeio_cursor_show();
            }
            continue;
        }

        if (key >= 32 && key < 127) {
            char c = (char)key;
            if (len + 1 < max_size) {
                dogeio_cursor_hide();

                for (size_t i = len + 1; i > pos; i--) {
                    buffer[i] = buffer[i - 1];
                }
                buffer[pos] = c;
                len++;
                pos++;
                buffer[len] = '\0';

                uint32_t saved_x = cursor_x;
                uint32_t saved_y = cursor_y;

                for (size_t i = pos - 1; i < len; i++) {
                    char ch = buffer[i];
                    text_grid[cursor_y * TERMINAL_COLS + cursor_x] = ch;
                    dogeio_putchar(ch, cursor_x, cursor_y);
                    cursor_x++;
                    if (cursor_x >= TERMINAL_COLS) {
                        cursor_x = 0;
                        cursor_y++;
                    }
                }

                cursor_x = saved_x;
                cursor_y = saved_y;
                
                cursor_x++;
                if (cursor_x >= TERMINAL_COLS) {
                    cursor_x = 0;
                    cursor_y++;
                }

                dogeio_cursor_show();
            }
            continue;
        }
    }

    dogeio_cursor_hide();
    buffer[len] = '\0';
}

void dogeio_enable_status_bar(void)
{
    status_bar_enabled = true;
}

void dogeio_disable_status_bar(void)
{
    status_bar_enabled = false;
}

bool dogeio_status_bar_enabled(void)
{
    return status_bar_enabled;
}