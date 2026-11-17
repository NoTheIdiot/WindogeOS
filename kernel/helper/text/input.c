#include <system.h>
#include <dogeio.h>
#include <bool.h>

#define USB_PID_IN 0x69
#define HID_REPORT_SIZE 8
#define KBD_QUEUE_SIZE 64

#define KEY_UP    0x101
#define KEY_DOWN  0x102
#define KEY_LEFT  0x103
#define KEY_RIGHT 0x104
#define KEY_ESC   0x105

extern uintptr_t hhdm_offset;

typedef struct uhci_td {
    uint32_t link_pointer;
    uint32_t control;
    uint32_t token;
    uint32_t buffer_pointer;
} __attribute__((packed, aligned(32))) uhci_td_t;

typedef struct uhci_qh {
    uint32_t horizontal_link;
    uint32_t element_link;
} __attribute__((packed, aligned(16))) uhci_qh_t;

typedef struct hid_keyboard_report {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycodes[6];
} __attribute__((packed)) hid_keyboard_report_t;

static uhci_qh_t *g_kbd_qh = NULL;
static uhci_td_t *g_kbd_td = NULL;
static volatile uint8_t *g_kbd_buffer = NULL;
static uint32_t g_kbd_buffer_phys = 0;
static uint32_t g_kbd_td_phys = 0;
static uint32_t g_kbd_qh_phys = 0;
static uint32_t g_keystroke_counter = 0;
static uint8_t g_last_key = 0;
static bool g_caps_lock = false;
static bool g_caps_pressed = false;

static uint8_t g_data_toggle = 0;
static uint8_t g_prev_keys[6] = {0};
static uint16_t g_key_queue[KBD_QUEUE_SIZE];
static volatile uint8_t g_q_head = 0;
static volatile uint8_t g_q_tail = 0;

static void kbd_enqueue(uint16_t key) {
    uint8_t next = (g_q_head + 1) % KBD_QUEUE_SIZE;
    if (next != g_q_tail) {
        g_key_queue[g_q_head] = key;
        g_q_head = next;
    }
}

static char hid_to_ascii(uint8_t keycode, bool shift, bool caps) {
    // is it shifted up, if shift holded or caps
    // on, then yes.
    bool upper = shift ^ caps;

    if (keycode >= 0x04 && keycode <= 0x1D) {
        return (upper ? 'A' : 'a') + (keycode - 0x04);
    }
    // numbers and it's shift counterpart
    else if (keycode >= 0x1E && keycode <= 0x26) {
        static const char normal[]  = "123456789";
        static const char shifted[] = "!@#$%^&*(";

        // is shift mate
        return shift ? shifted[keycode - 0x1E]
                     : normal[keycode - 0x1E];
    }

    // some keys that have a shift alternative
    else if (keycode == 0x27) {
        return shift ? ')' : '0';
    }
    else if (keycode == 0x2C) {
        return ' ';
    }
    else if (keycode == 0x2D) {
        return shift ? '_' : '-';
    }
    else if (keycode == 0x2E) {
        return shift ? '+' : '=';
    }
    else if (keycode == 0x2F) {
        return shift ? '{' : '[';
    }
    else if (keycode == 0x30) {
        return shift ? '}' : ']';
    }
    else if (keycode == 0x31) {
        return shift ? '|' : '\\';
    }
    else if (keycode == 0x33) {
        return shift ? ':' : ';';
    }
    else if (keycode == 0x34) {
        return shift ? '"' : '\'';
    }
    else if (keycode == 0x35) {
        return shift ? '~' : '`';
    }
    else if (keycode == 0x36) {
        return shift ? '<' : ',';
    }
    else if (keycode == 0x37) {
        return shift ? '>' : '.';
    }
    else if (keycode == 0x38) {
        return shift ? '?' : '/';
    }
    else if (keycode == 0x28) {
        return '\n';
    }
    else if (keycode == 0x2A) {
        return '\b';
    }

    return 0;
}

bool uhci_init_keyboard_endpoint(uint8_t device_address, uint8_t endpoint_num) {
   // replacment
    uintptr_t buf_phys = alloc_page();
    if (!buf_phys || buf_phys > 0xFFFFFFFF) {
        if (buf_phys) free_page(buf_phys);
        return false;
    }

    uintptr_t qh_phys = alloc_page();
    if (!qh_phys || qh_phys > 0xFFFFFFFF) {
        if (qh_phys) free_page(qh_phys);
        free_page(buf_phys);
        return false;
    }

    uintptr_t td_phys = alloc_page();
    if (!td_phys || td_phys > 0xFFFFFFFF) {
        if (td_phys) free_page(td_phys);
        free_page(qh_phys);
        free_page(buf_phys);
        return false;
    }

    g_kbd_buffer_phys = (uint32_t)buf_phys;
    g_kbd_buffer = (volatile uint8_t *)(buf_phys + hhdm_offset);

    g_kbd_qh_phys = (uint32_t)qh_phys;
    g_kbd_qh = (uhci_qh_t *)(qh_phys + hhdm_offset);

    g_kbd_td_phys = (uint32_t)td_phys;
    g_kbd_td = (uhci_td_t *)(td_phys + hhdm_offset);

    g_data_toggle = 0;

    uint32_t max_len = HID_REPORT_SIZE;
    g_kbd_td->link_pointer = 0x00000001;
    g_kbd_td->control = (1 << 26) | (3 << 27);
    g_kbd_td->token = ((max_len - 1) << 21) | (0 << 19) | ((endpoint_num & 0xF) << 15) | ((device_address & 0x7F) << 8) | USB_PID_IN;
    g_kbd_td->buffer_pointer = g_kbd_buffer_phys;

    g_kbd_qh->horizontal_link = 0x00000001;
    g_kbd_qh->element_link = g_kbd_td_phys;

    extern uint32_t *g_frame_list_virt;
    if (g_frame_list_virt) {
        for (int i = 0; i < 1024; i++) {
            g_frame_list_virt[i] = g_kbd_qh_phys | 0x2;
        }
    }

    g_kbd_td->control |= (1 << 23);

    sprintln("[UHCI] Holy shit the keyboard is no longer in a coma");
    return true;
}

void init_usb_keyboard(uint8_t device_address) {
    sprintln("[UHCI] Invoking ritual for device address");
    uhci_init_keyboard_endpoint(device_address, 1);
}

void poll_usb_keyboard(void)
{
    volatile uhci_td_t *td = g_kbd_td;

    if (!td || !g_kbd_buffer || !g_kbd_qh)
        return;

    uint32_t control = td->control;

    if (control & (1 << 23))
        return;

    if (control & 0x007E0000)
    {
        td->buffer_pointer = g_kbd_buffer_phys;
        g_kbd_qh->element_link = g_kbd_td_phys;

        td->control =
            (1 << 26) |
            (3 << 27) |
            (1 << 23);

        return;
    }

    g_data_toggle ^= 1;
    td->token = (td->token & ~(1U << 19)) | ((uint32_t)g_data_toggle << 19);

    hid_keyboard_report_t *report =
        (hid_keyboard_report_t *)g_kbd_buffer;

    // hey USB, shift on in keyboard?
    bool shift = (report->modifiers & 0x22) != 0;

    bool caps_key_down = false;

    // hey there USB, caps on?
    for (int i = 0; i < 6; i++) {
        if (report->keycodes[i] == 0x39) {
            // yeah dude
            caps_key_down = true;
            break;
        }
    }
    // fuh nah

    if (caps_key_down && !g_caps_pressed)
        g_caps_lock = !g_caps_lock;

    g_caps_pressed = caps_key_down;

    for (int i = 0; i < 6; i++) {
        uint8_t kc = report->keycodes[i];
        if (kc == 0 || kc == 0x39)
            continue;

        bool was_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (g_prev_keys[j] == kc) {
                was_pressed = true;
                break;
            }
        }

        if (!was_pressed) {
            g_last_key = kc;
            g_keystroke_counter++;

            char c = hid_to_ascii(kc, shift, g_caps_lock);
            if (c) {
                kbd_enqueue((uint8_t)c);
            } else {
                switch (kc) {
                    case 0x29: kbd_enqueue(KEY_ESC);   break;
                    case 0x4F: kbd_enqueue(KEY_RIGHT); break;
                    case 0x50: kbd_enqueue(KEY_LEFT);  break;
                    case 0x51: kbd_enqueue(KEY_DOWN);  break;
                    case 0x52: kbd_enqueue(KEY_UP);    break;
                }
            }
        }
    }

    for (int i = 0; i < 6; i++) {
        g_prev_keys[i] = report->keycodes[i];
    }

    td->buffer_pointer = g_kbd_buffer_phys;
    g_kbd_qh->element_link = g_kbd_td_phys;

    td->control =
        (1 << 26) |
        (3 << 27) |
        (1 << 23);
}

uint16_t get_key(void)
{
    poll_usb_keyboard();

    if (g_q_head == g_q_tail)
        return 0;

    uint16_t key = g_key_queue[g_q_tail];
    g_q_tail = (g_q_tail + 1) % KBD_QUEUE_SIZE;
    return key;
}

void input(char* prompt, char* buffer)
{
    print(prompt);

    int idx = 0;

    while (1) {

        // get a key.
        uint16_t key = get_key();

        if (!key)
            continue;

        // ignore non-ascii special keys in line input
        if (key >= 0x100)
            continue;

        char c = (char)key;

        if (!c)
            continue;

        // is newline? thats enter so finish
        if (c == '\n') {
            buffer[idx] = '\0';
            printchar('\n');
            break;
        }

        // backspace? go back and clear
        else if (c == '\b') {
            if (idx > 0) {
                idx--;

                printchar('\b');
                printchar(' ');
                printchar('\b');
            }
        }

        // normal phooking keys
        else {
            buffer[idx++] = c;
            printchar(c);
        }
    }
}
