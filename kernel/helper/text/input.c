#include <system.h>
#include <dogeio.h>
#include <limine.h>

#define USB_PID_IN 0x69
#define HID_REPORT_SIZE 8

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

extern volatile struct limine_hhdm_request hhdm_request;

bool uhci_init_keyboard_endpoint(uint8_t device_address, uint8_t endpoint_num) {
    uint64_t hhdm_offset = hhdm_request.response->offset;

    uint64_t buf_phys = pmm_alloc_zeroed_page();
    if (buf_phys > 0xFFFFFFFF) {
        return false;
    }
    g_kbd_buffer_phys = (uint32_t)buf_phys;
    g_kbd_buffer = (volatile uint8_t *)(buf_phys + hhdm_offset);

    uint64_t qh_phys = pmm_alloc_zeroed_page();
    g_kbd_qh_phys = (uint32_t)qh_phys;
    g_kbd_qh = (uhci_qh_t *)(qh_phys + hhdm_offset);

    uint64_t td_phys = pmm_alloc_zeroed_page();
    g_kbd_td_phys = (uint32_t)td_phys;
    g_kbd_td = (uhci_td_t *)(td_phys + hhdm_offset);

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

    sprintln("[UHCI] Keyboard tether established. The silicon hums.");
    return true;
}

void init_usb_keyboard(uint8_t device_address) {
    sprintln("[UHCI] Invoking ritual for device address");
    uhci_init_keyboard_endpoint(device_address, 1);
}

uint16_t get_key(void)
{
    if (!g_kbd_td || !g_kbd_buffer || !g_kbd_qh)
        return 0;

    uint32_t control = g_kbd_td->control;

    if (!(control & (1 << 23))) {

        if (control & 0x7B0000) {
            g_kbd_td->buffer_pointer = g_kbd_buffer_phys;
            g_kbd_qh->element_link = g_kbd_td_phys;
            g_kbd_td->control =
                (1 << 26) |
                (3 << 27) |
                (1 << 23);
            return 0;
        }

        hid_keyboard_report_t *report =
            (hid_keyboard_report_t *)g_kbd_buffer;

        uint8_t current_key = 0;

        for (int i = 0; i < 6; i++) {
            if (report->keycodes[i]) {
                current_key = report->keycodes[i];
                break;
            }
        }

        uint16_t key = 0;

        if (current_key && current_key != g_last_key)
            key = current_key;

        g_last_key = current_key;

        g_kbd_td->buffer_pointer = g_kbd_buffer_phys;
        g_kbd_td->token ^= (1 << 19);
        g_kbd_qh->element_link = g_kbd_td_phys;
        g_kbd_td->control =
            (1 << 26) |
            (3 << 27) |
            (1 << 23);

        return key;
    }

    return 0;
}

void input(char* prompt, char* buffer)
{
    print(prompt);

    int idx = 0;

    while (1) {

        uint16_t key = get_key();

        if (!key)
            continue;

        uint8_t keycode = (uint8_t)key;
        char c = 0;

        if (keycode >= 0x04 && keycode <= 0x1D) {
            c = 'a' + (keycode - 0x04);
        } else if (keycode >= 0x1E && keycode <= 0x26) {
            c = '1' + (keycode - 0x1E);
        } else if (keycode == 0x27) {
            c = '0';
        } else if (keycode == 0x2C) {
            c = ' ';
        } else if (keycode == 0x28) {
            c = '\n';
        } else if (keycode == 0x2A) {
            c = '\b';
        }

        if (!c)
            continue;

        if (c == '\n') {
            buffer[idx] = '\0';
            printchar('\n');
            break;
        }
        else if (c == '\b') {
            if (idx > 0) {
                idx--;

                printchar('\b');
                printchar(' ');
                printchar('\b');
            }
        }
        else {
            buffer[idx++] = c;
            printchar(c);
        }
    }
}