#include <system.h>
#include <stdint.h>
#include <stdbool.h>

extern uintptr_t hhdm_offset;
extern uint32_t *g_frame_list_virt;
extern bool uhci_send_control(uint8_t dev_addr, uint8_t req_type, uint8_t request, uint16_t value, uint16_t index, uint16_t length);

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

static uhci_td_t *g_mouse_td = NULL;
static uhci_qh_t *g_mouse_qh = NULL;
static volatile uint8_t *g_mouse_buf = NULL;
static uint32_t g_mouse_td_phys = 0;
static uint32_t g_mouse_buf_phys = 0;

static uint8_t g_mouse_addr = 0;
static uint8_t g_mouse_toggle = 0;

static int32_t g_screen_w = 1024;
static int32_t g_screen_h = 768;
static int32_t g_mouse_x = 512;
static int32_t g_mouse_y = 384;
static uint8_t g_mouse_buttons = 0;

static void arm_mouse_td(void) {
    g_mouse_td->link_pointer = 0x00000001;
    g_mouse_td->control = (1 << 26) | (3 << 27) | (1 << 23);
    g_mouse_td->token = (3 << 21) | ((g_mouse_toggle & 1) << 19) | (1 << 15) | ((g_mouse_addr & 0x7F) << 8) | 0x69;
    g_mouse_td->buffer_pointer = g_mouse_buf_phys;
    g_mouse_qh->element_link = g_mouse_td_phys;
}

void init_usb_mouse(uint8_t dev_addr, int screen_w, int screen_h) {
    if (dev_addr == 0 || !g_frame_list_virt) return;
    g_mouse_addr = dev_addr;
    if (screen_w > 0) g_screen_w = screen_w;
    if (screen_h > 0) g_screen_h = screen_h;
    g_mouse_x = g_screen_w / 2;
    g_mouse_y = g_screen_h / 2;

    uhci_send_control(g_mouse_addr, 0x21, 0x0B, 0, 0, 0);
    uhci_send_control(g_mouse_addr, 0x21, 0x0A, 0, 0, 0);

    uintptr_t td_phys  = alloc_page();
    uintptr_t qh_phys  = alloc_page();
    uintptr_t buf_phys = alloc_page();
    if (!td_phys || !qh_phys || !buf_phys) return;

    g_mouse_td_phys  = (uint32_t)td_phys;
    g_mouse_buf_phys = (uint32_t)buf_phys;

    g_mouse_td  = (uhci_td_t *)(td_phys + hhdm_offset);
    g_mouse_qh  = (uhci_qh_t *)(qh_phys + hhdm_offset);
    g_mouse_buf = (volatile uint8_t *)(buf_phys + hhdm_offset);

    g_mouse_toggle = 0;
    arm_mouse_td();

    uint32_t existing_qh = g_frame_list_virt[0];
    g_mouse_qh->horizontal_link = existing_qh;

    for (int i = 0; i < 1024; i++) {
        g_frame_list_virt[i] = (uint32_t)qh_phys | 0x2;
    }
}

void poll_usb_mouse(void) {
    if (!g_mouse_td) return;

    if (!(g_mouse_td->control & (1 << 23))) {
        if (!(g_mouse_td->control & 0x7B0000)) {
            g_mouse_buttons = g_mouse_buf[0];
            int8_t dx = (int8_t)g_mouse_buf[1];
            int8_t dy = (int8_t)g_mouse_buf[2];

            g_mouse_x += dx;
            g_mouse_y += dy;

            if (g_mouse_x < 0) g_mouse_x = 0;
            if (g_mouse_y < 0) g_mouse_y = 0;
            if (g_mouse_x >= g_screen_w) g_mouse_x = g_screen_w - 1;
            if (g_mouse_y >= g_screen_h) g_mouse_y = g_screen_h - 1;

            g_mouse_toggle ^= 1;
        }
        arm_mouse_td();
    }
}

int32_t get_mouse_x(void) { return g_mouse_x; }
int32_t get_mouse_y(void) { return g_mouse_y; }
uint8_t get_mouse_buttons(void) { return g_mouse_buttons; }
