#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <dogeio.h>

#define XHCI_MAX_SLOTS          32
#define XHCI_RING_TRBS          64
#define XHCI_MAX_PORTS          128
#define XHCI_SPIN_TIMEOUT       5000000U

#define PCI_CLASS_SERIAL        0x0C
#define PCI_SUBCLASS_USB        0x03
#define PCI_PROGIF_XHCI         0x30

// USBCMD bits
#define XHCI_CMD_RUN            (1U << 0)
#define XHCI_CMD_HCRST          (1U << 1)
#define XHCI_CMD_INTE           (1U << 2)

// USBSTS bits
#define XHCI_STS_HCH            (1U << 0)
#define XHCI_STS_EINT           (1U << 3)
#define XHCI_STS_CNR            (1U << 11)

// PORTSC bits
#define XHCI_PORTSC_CCS         (1U << 0)
#define XHCI_PORTSC_PED         (1U << 1)
#define XHCI_PORTSC_PR          (1U << 4)
#define XHCI_PORTSC_PP          (1U << 9)
#define XHCI_PORTSC_CSC         (1U << 17)
#define XHCI_PORTSC_PEC         (1U << 18)
#define XHCI_PORTSC_PRC         (1U << 21)
#define XHCI_PORTSC_W1C_MASK    ((0x7FU << 17) | XHCI_PORTSC_PED)

// TRB types
#define TRB_TYPE_NORMAL         1
#define TRB_TYPE_LINK           6
#define TRB_TYPE_ENABLE_SLOT    9
#define TRB_TYPE_ADDRESS_DEVICE 11
#define TRB_TYPE_CONFIGURE_EP   12
#define TRB_TYPE_TRANSFER_EVENT 32
#define TRB_TYPE_CMD_COMPLETION 33
#define TRB_TYPE_PORT_STATUS    34

#define TRB_LINK_TC             (1U << 1)
#define TRB_TR_IOC              (1U << 5)

// Completion codes
#define TRB_CC_SUCCESS          1
#define TRB_CC_SHORT_PACKET     13

#define USB_SPEED_FULL          1
#define USB_SPEED_LOW           2
#define USB_SPEED_HIGH          3
#define USB_SPEED_SUPER         4

#define HID_MOD_LCTRL           (1U << 0)
#define HID_MOD_LSHIFT          (1U << 1)
#define HID_MOD_LALT            (1U << 2)
#define HID_MOD_RCTRL           (1U << 4)
#define HID_MOD_RSHIFT          (1U << 5)
#define HID_MOD_RALT            (1U << 6)

// more DAMN TYPEDEF STRUCTS
typedef struct __attribute__((packed)) {
    uint8_t  caplength;
    uint8_t  reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;
} xhci_cap_regs_t;

typedef struct __attribute__((packed)) {
    uint32_t usbcmd;
    uint32_t usbsts;
    uint32_t pagesize;
    uint32_t rsv0[2];
    uint32_t dnctrl;
    uint64_t crcr;
    uint32_t rsv1[4];
    uint64_t dcbaap;
    uint32_t config;
    uint8_t  rsv2[0x3F4];
    uint32_t portsc[XHCI_MAX_PORTS];
} xhci_op_regs_t;

typedef struct __attribute__((packed)) {
    uint32_t iman;
    uint32_t imod;
    uint32_t erstsz;
    uint32_t rsv0;
    uint64_t erstba;
    uint64_t erdp;
} xhci_interrupter_t;

typedef struct __attribute__((packed)) {
    uint32_t mfindex;
    uint32_t rsv0[7];
    xhci_interrupter_t ir[1];
} xhci_rt_regs_t;

typedef struct __attribute__((packed)) {
    uint64_t param;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

typedef struct __attribute__((packed)) {
    uint64_t ring_base_phys;
    uint32_t ring_size;
    uint32_t rsv;
} xhci_erst_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsv[6];
} xhci_input_ctrl_ctx_t;

typedef struct __attribute__((packed)) {
    uint32_t info1;
    uint32_t info2;
    uint32_t tt_info;
    uint32_t dev_state;
    uint32_t rsv[4];
} xhci_slot_ctx_t;

typedef struct __attribute__((packed)) {
    uint32_t ep_state;
    uint32_t ep_info;
    uint64_t tr_dequeue;
    uint32_t tx_len;
    uint32_t rsv[3];
} xhci_ep_ctx_t;

typedef struct __attribute__((packed)) {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t   ep[31];
} xhci_dev_ctx_t;

typedef struct __attribute__((packed)) {
    xhci_input_ctrl_ctx_t ctrl;
    xhci_slot_ctx_t       slot;
    xhci_ep_ctx_t         ep[31];
} xhci_input_ctx_t;

typedef struct {
    xhci_trb_t *trbs;
    uint64_t    phys;
    uint32_t    enqueue;
    uint8_t     cycle;
} xhci_ring_t;

// global variables to keep state
static volatile xhci_op_regs_t *g_op = NULL;
static volatile xhci_rt_regs_t *g_rt = NULL;
static volatile uint32_t *g_db = NULL;

static uint64_t *g_dcbaa_phys_tbl = NULL;
static uint64_t  g_dcbaa_phys = 0;

static xhci_ring_t g_cmd_ring;
static xhci_ring_t g_ep0_ring;
static xhci_ring_t g_ep1_in_ring;

static xhci_trb_t *g_ev_ring = NULL;
static uint64_t    g_ev_ring_phys = 0;
static uint32_t    g_ev_dequeue = 0;
static uint8_t     g_ev_cycle = 1;

static uint64_t  g_erst_phys = 0;
static uint8_t   g_max_slots = 0;
static uint8_t   g_max_ports = 0;
static uint8_t   g_kbd_slot = 0;
static uint8_t  *g_kbd_report_virt = NULL;
static uint64_t  g_kbd_report_phys = 0;
static uint8_t   g_prev_keys[6];
static bool      g_caps_lock = false;

// read and write xhci controller
static inline uint32_t xhci_r32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void xhci_w32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

static inline void xhci_w64(uintptr_t addr, uint64_t val) {
    *(volatile uint32_t *)addr = (uint32_t)val;
    *(volatile uint32_t *)(addr + 4) = (uint32_t)(val >> 32);
}

static inline uintptr_t xhci_portsc_addr(uint8_t port_idx) {
    uintptr_t op_base = (uintptr_t)g_op;
    return op_base + 0x400U + ((uintptr_t)port_idx * 0x10U);
}

static void *alloc_zero_page(uint64_t *out_phys) {
    uint64_t phys = alloc_page();
    if (!phys) return NULL;

    void *virt = map_kernel_mmio(phys, PAGE_SIZE);
    if (!virt) return NULL;

    memset(virt, 0, PAGE_SIZE);
    if (out_phys) *out_phys = phys;
    return virt;
}

static int ring_init(xhci_ring_t *ring) {
    ring->trbs = (xhci_trb_t *)alloc_zero_page(&ring->phys);
    if (!ring->trbs) return -1;

    ring->enqueue = 0;
    ring->cycle = 1;

    xhci_trb_t *link = &ring->trbs[XHCI_RING_TRBS - 1];
    link->param = ring->phys;
    link->status = 0;
    link->control = (TRB_TYPE_LINK << 10) | TRB_LINK_TC;
    return 0;
}

static void ring_wrap_if_needed(xhci_ring_t *ring) {
    if (ring->enqueue == XHCI_RING_TRBS - 1) {
        xhci_trb_t *link = &ring->trbs[XHCI_RING_TRBS - 1];
        link->param = ring->phys;
        link->status = 0;
        link->control = (TRB_TYPE_LINK << 10) | TRB_LINK_TC | (ring->cycle & 1U);
        ring->enqueue = 0;
        ring->cycle ^= 1U;
    }
}

static void ring_enqueue(xhci_ring_t *ring, uint64_t param, uint32_t status, uint32_t trb_type, uint32_t extra_ctrl) {
    ring_wrap_if_needed(ring);

    xhci_trb_t *trb = &ring->trbs[ring->enqueue];
    trb->param = param;
    trb->status = status;
    uint32_t ctrl = (trb_type << 10) | (extra_ctrl & ~1U);
    trb->control = ctrl;
    __asm__ volatile("" ::: "memory");
    trb->control = ctrl | (ring->cycle & 1U);

    ring->enqueue++;
    ring_wrap_if_needed(ring);
}

static void ring_cmd(uint64_t param, uint32_t status, uint32_t trb_type, uint32_t extra_ctrl) {
    ring_enqueue(&g_cmd_ring, param, status, trb_type, extra_ctrl);
    xhci_w32((uintptr_t)&g_db[0], 0);
}

static void update_erdp(void) {
    uint64_t deq_phys = g_ev_ring_phys + ((uint64_t)g_ev_dequeue * sizeof(xhci_trb_t));
    xhci_w64((uintptr_t)&g_rt->ir[0].erdp, deq_phys | (1ULL << 3));
}

static int wait_cmd_completion(uint8_t *out_slot, uint8_t *out_cc) {
    for (uint32_t spin = 0; spin < XHCI_SPIN_TIMEOUT; spin++) {
        xhci_trb_t *ev = &g_ev_ring[g_ev_dequeue];
        if ((ev->control & 1U) != g_ev_cycle) {
            continue;
        }

        uint32_t type = (ev->control >> 10) & 0x3FU;
        uint8_t cc = (ev->status >> 24) & 0xFFU;
        uint8_t slot_id = (ev->control >> 24) & 0xFFU;

        g_ev_dequeue++;
        if (g_ev_dequeue == XHCI_RING_TRBS) {
            g_ev_dequeue = 0;
            g_ev_cycle ^= 1U;
        }
        update_erdp();

        if (type == TRB_TYPE_CMD_COMPLETION) {
            if (out_slot) *out_slot = slot_id;
            if (out_cc) *out_cc = cc;
            return (cc == TRB_CC_SUCCESS) ? 0 : -1;
        }
    }
    return -1;
}

static uint32_t usb_speed_to_mps(uint32_t speed) {
    switch (speed) {
        case USB_SPEED_LOW:   return 8;
        case USB_SPEED_FULL:  return 8;
        case USB_SPEED_HIGH:  return 64;
        case USB_SPEED_SUPER: return 512;
        default:              return 8;
    }
}

static void queue_kbd_interrupt_in(void) {
    memset(g_kbd_report_virt, 0, 8);
    ring_enqueue(&g_ep1_in_ring, g_kbd_report_phys, 8U, TRB_TYPE_NORMAL, TRB_TR_IOC);
    xhci_w32((uintptr_t)&g_db[g_kbd_slot], 3U);
}

static int setup_usb_keyboard(uint8_t port_idx) {
    uint32_t portsc = xhci_r32(xhci_portsc_addr(port_idx));
    uint32_t speed = (portsc >> 10) & 0x0FU;
    if (speed == 0) speed = USB_SPEED_FULL;
    uint32_t mps = usb_speed_to_mps(speed);

    uint8_t slot_id = 0, cc = 0;
    ring_cmd(0, 0, TRB_TYPE_ENABLE_SLOT, 0);
    if (wait_cmd_completion(&slot_id, &cc) != 0 || slot_id == 0 || slot_id > g_max_slots) {
        sprintf("[xHCI] Enable Slot failed (slot=%u, cc=%u)\n", slot_id, cc);
        return -1;
    }

    uint64_t dev_ctx_phys = 0;
    xhci_dev_ctx_t *dev_ctx = (xhci_dev_ctx_t *)alloc_zero_page(&dev_ctx_phys);
    if (!dev_ctx) return -1;
    g_dcbaa_phys_tbl[slot_id] = dev_ctx_phys;

    if (ring_init(&g_ep0_ring) != 0) return -1;

    uint64_t in_ctx_phys = 0;
    xhci_input_ctx_t *in_ctx = (xhci_input_ctx_t *)alloc_zero_page(&in_ctx_phys);
    if (!in_ctx) return -1;

    // Context Entries = 1 (EP0), Speed in info1[23:20], Root Hub Port Number (1-based) in info2[23:16]
    in_ctx->ctrl.add_flags = (1U << 0) | (1U << 1);
    in_ctx->slot.info1 = (1U << 27) | (speed << 20);
    in_ctx->slot.info2 = ((uint32_t)(port_idx + 1) << 16);
    in_ctx->ep[0].ep_info = (mps << 16) | (4U << 3) | (3U << 1);
    in_ctx->ep[0].tr_dequeue = g_ep0_ring.phys | 1ULL;
    in_ctx->ep[0].tx_len = mps;

    ring_cmd(in_ctx_phys, 0, TRB_TYPE_ADDRESS_DEVICE, (uint32_t)slot_id << 24);
    if (wait_cmd_completion(NULL, &cc) != 0) {
        sprintf("[xHCI] Address Device failed on slot %u (cc=%u)\n", slot_id, cc);
        return -1;
    }

    if (ring_init(&g_ep1_in_ring) != 0) return -1;

    memset(in_ctx, 0, sizeof(*in_ctx));
    in_ctx->ctrl.add_flags = (1U << 0) | (1U << 3);
    in_ctx->slot.info1 = (3U << 27) | (speed << 20);
    in_ctx->slot.info2 = ((uint32_t)(port_idx + 1) << 16);

    uint32_t interval = (speed == USB_SPEED_HIGH || speed == USB_SPEED_SUPER) ? 6U : 3U;
    in_ctx->ep[2].ep_info = (interval << 16) | (8U << 16) | (7U << 3) | (3U << 1);
    in_ctx->ep[2].tr_dequeue = g_ep1_in_ring.phys | 1ULL;
    in_ctx->ep[2].tx_len = 8U;

    ring_cmd(in_ctx_phys, 0, TRB_TYPE_CONFIGURE_EP, (uint32_t)slot_id << 24);
    if (wait_cmd_completion(NULL, &cc) != 0) {
        sprintf("[xHCI] Configure EP1 IN failed on slot %u (cc=%u)\n", slot_id, cc);
        return -1;
    }

    g_kbd_report_virt = (uint8_t *)alloc_zero_page(&g_kbd_report_phys);
    if (!g_kbd_report_virt) return -1;

    memset(g_prev_keys, 0, sizeof(g_prev_keys));
    g_caps_lock = false;
    g_kbd_slot = slot_id;

    queue_kbd_interrupt_in();
    return 0;
}

static char hid_usage_to_ascii(uint8_t usage, bool shift, bool caps) {
    static const char lower[128] = {
        [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd', [0x08] = 'e', [0x09] = 'f',
        [0x0A] = 'g', [0x0B] = 'h', [0x0C] = 'i', [0x0D] = 'j', [0x0E] = 'k', [0x0F] = 'l',
        [0x10] = 'm', [0x11] = 'n', [0x12] = 'o', [0x13] = 'p', [0x14] = 'q', [0x15] = 'r',
        [0x16] = 's', [0x17] = 't', [0x18] = 'u', [0x19] = 'v', [0x1A] = 'w', [0x1B] = 'x',
        [0x1C] = 'y', [0x1D] = 'z',
        [0x1E] = '1', [0x1F] = '2', [0x20] = '3', [0x21] = '4', [0x22] = '5', [0x23] = '6',
        [0x24] = '7', [0x25] = '8', [0x26] = '9', [0x27] = '0',
        [0x28] = '\n', [0x29] = 27, [0x2A] = '\b', [0x2B] = '\t', [0x2C] = ' ',
        [0x2D] = '-', [0x2E] = '=', [0x2F] = '[', [0x30] = ']', [0x31] = '\\',
        [0x33] = ';', [0x34] = '\'', [0x35] = '`', [0x36] = ',', [0x37] = '.', [0x38] = '/',
        [0x4F] = KEY_RIGHT, [0x50] = KEY_LEFT, [0x51] = KEY_DOWN, [0x52] = KEY_UP
    };
    static const char upper[128] = {
        [0x04] = 'A', [0x05] = 'B', [0x06] = 'C', [0x07] = 'D', [0x08] = 'E', [0x09] = 'F',
        [0x0A] = 'G', [0x0B] = 'H', [0x0C] = 'I', [0x0D] = 'J', [0x0E] = 'K', [0x0F] = 'L',
        [0x10] = 'M', [0x11] = 'N', [0x12] = 'O', [0x13] = 'P', [0x14] = 'Q', [0x15] = 'R',
        [0x16] = 'S', [0x17] = 'T', [0x18] = 'U', [0x19] = 'V', [0x1A] = 'W', [0x1B] = 'X',
        [0x1C] = 'Y', [0x1D] = 'Z',
        [0x1E] = '!', [0x1F] = '@', [0x20] = '#', [0x21] = '$', [0x22] = '%', [0x23] = '^',
        [0x24] = '&', [0x25] = '*', [0x26] = '(', [0x27] = ')',
        [0x28] = '\n', [0x29] = 27, [0x2A] = '\b', [0x2B] = '\t', [0x2C] = ' ',
        [0x2D] = '_', [0x2E] = '+', [0x2F] = '{', [0x30] = '}', [0x31] = '|',
        [0x33] = ':', [0x34] = '"', [0x35] = '~', [0x36] = '<', [0x37] = '>', [0x38] = '?',
        [0x4F] = KEY_RIGHT, [0x50] = KEY_LEFT, [0x51] = KEY_DOWN, [0x52] = KEY_UP
    };

    if (usage >= 128) return 0;

    bool is_alpha = (usage >= 0x04 && usage <= 0x1D);
    if (is_alpha) {
        bool effective_upper = shift ^ caps;
        return effective_upper ? upper[usage] : lower[usage];
    }
    return shift ? upper[usage] : lower[usage];
}

char get_key(void) {
    if (!g_op || !g_ev_ring || g_kbd_slot == 0) {
        return 0;
    }

    char out_char = 0;
    while (1) {
        xhci_trb_t *ev = &g_ev_ring[g_ev_dequeue];
        if ((ev->control & 1U) != g_ev_cycle) {
            break;
        }

        uint32_t type = (ev->control >> 10) & 0x3FU;
        uint8_t slot_id = (ev->control >> 24) & 0xFFU;

        g_ev_dequeue++;
        if (g_ev_dequeue == XHCI_RING_TRBS) {
            g_ev_dequeue = 0;
            g_ev_cycle ^= 1U;
        }
        update_erdp();

        if (type == TRB_TYPE_TRANSFER_EVENT && slot_id == g_kbd_slot) {
            uint8_t mods = g_kbd_report_virt[0];
            bool shift = (mods & (HID_MOD_LSHIFT | HID_MOD_RSHIFT)) != 0;

            for (int i = 2; i < 8; i++) {
                uint8_t key = g_kbd_report_virt[i];
                if (key == 0 || key == 1) continue;

                bool was_pressed = false;
                for (int p = 0; p < 6; p++) {
                    if (g_prev_keys[p] == key) {
                        was_pressed = true;
                        break;
                    }
                }

                if (!was_pressed) {
                    if (key == 0x39) {
                        g_caps_lock = !g_caps_lock;
                    } else if (out_char == 0) {
                        char c = hid_usage_to_ascii(key, shift, g_caps_lock);
                        bool ctrl = (mods & (HID_MOD_LCTRL | HID_MOD_RCTRL)) != 0;
                        if (ctrl &&
                            c >= 'a' &&
                            c <= 'z')
                        {
                            c = c - 'a' + 1;
                        }
                        else if (
                            ctrl &&
                            c >= 'A' &&
                            c <= 'Z')
                        {
                            c = c - 'A' + 1;
                        }
                        if (c) out_char = c;
                    }
                }
            }
            memcpy(g_prev_keys, &g_kbd_report_virt[2], 6);

            queue_kbd_interrupt_in();
        }
    }

    return out_char;
}

size_t input_masked(const char *prompt, char *buf, size_t len, bool mask) {
    if (!buf || len == 0) return 0;

    if (prompt) {
        printf("%s", prompt);
    }

    size_t idx = 0;
    while (1) {
        char c = get_key();
        if (!c) continue;

        if (c == '\r' || c == '\n') {
            putchar('\n');
            buf[idx] = '\0';
            return idx;
        }
        if ((c == '\b' || c == 127) && idx > 0) {
            idx--;
            printf("\b \b");
            continue;
        }
        if (c >= 32 && c <= 126 && idx + 1 < len) {
            buf[idx++] = c;
            putchar(mask ? '*' : c);
        }
    }
}

size_t input(const char *prompt, char *buf, size_t len) {
    return input_masked(prompt, buf, len, false);
}

void wait_for_next(void) {
    while (true) {
        if (get_key() != 0) {
            break;
        }
    }
}

static bool find_xhci_pci(uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func, uint64_t *out_bar0) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read_32((uint8_t)bus, slot, 0, 0x00);
            if ((id & 0xFFFFU) == 0xFFFFU) continue;

            uint8_t hdr_type = pci_read_8((uint8_t)bus, slot, 0, 0x0E);
            uint8_t max_func = (hdr_type & 0x80U) ? 8 : 1;

            for (uint8_t func = 0; func < max_func; func++) {
                id = pci_read_32((uint8_t)bus, slot, func, 0x00);
                if ((id & 0xFFFFU) == 0xFFFFU) continue;

                uint32_t cc = pci_read_32((uint8_t)bus, slot, func, 0x08);
                uint8_t base_class = (cc >> 24) & 0xFFU;
                uint8_t sub_class  = (cc >> 16) & 0xFFU;
                uint8_t prog_if    = (cc >> 8) & 0xFFU;

                if (base_class == PCI_CLASS_SERIAL &&
                    sub_class == PCI_SUBCLASS_USB &&
                    prog_if == PCI_PROGIF_XHCI) {
                    pci_bar_t bar0 = pci_get_bar((uint8_t)bus, slot, func, 0);
                    if (bar0.address != 0) {
                        *out_bus = (uint8_t)bus;
                        *out_slot = slot;
                        *out_func = func;
                        *out_bar0 = bar0.address;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static int wait_mmio_clear(uintptr_t addr, uint32_t mask) {
    for (uint32_t i = 0; i < XHCI_SPIN_TIMEOUT; i++) {
        if (!(xhci_r32(addr) & mask)) return 0;
    }
    return -1;
}

static int wait_mmio_set(uintptr_t addr, uint32_t mask) {
    for (uint32_t i = 0; i < XHCI_SPIN_TIMEOUT; i++) {
        if (xhci_r32(addr) & mask) return 0;
    }
    return -1;
}

int init_keyboard(void) {
    uint8_t bus = 0, slot = 0, func = 0;
    uint64_t bar0_phys = 0;
    if (!find_xhci_pci(&bus, &slot, &func, &bar0_phys)) {
        sprintf("[xHCI] Controller not found.\n");
        return -1;
    }

    pci_enable_device(bus, slot, func);

    void *mmio_base = map_kernel_mmio(bar0_phys, PAGE_SIZE * 4);
    if (!mmio_base) {
        sprintf("[xHCI] Failed to map BAR0 MMIO.\n");
        return -1;
    }

    uintptr_t cap_base = (uintptr_t)mmio_base;
    uint8_t caplen = *(volatile uint8_t *)cap_base;
    uint32_t hcs1 = xhci_r32(cap_base + offsetof(xhci_cap_regs_t, hcsparams1));
    uint32_t dboff = xhci_r32(cap_base + offsetof(xhci_cap_regs_t, dboff)) & ~0x3U;
    uint32_t rtsoff = xhci_r32(cap_base + offsetof(xhci_cap_regs_t, rtsoff)) & ~0x1FU;

    size_t needed_end = (size_t)rtsoff + sizeof(xhci_rt_regs_t);
    size_t db_end = (size_t)dboff + (XHCI_MAX_SLOTS * sizeof(uint32_t));
    if (db_end > needed_end) needed_end = db_end;
    if (needed_end > PAGE_SIZE * 4) {
        size_t pages = (needed_end + PAGE_SIZE - 1) / PAGE_SIZE;
        void *remap = map_kernel_mmio(bar0_phys, PAGE_SIZE * pages);
        if (!remap) {
            sprintf("[xHCI] Failed to map full MMIO range.\n");
            return -1;
        }
        cap_base = (uintptr_t)remap;
    }

    g_max_slots = (uint8_t)(hcs1 & 0xFFU);
    if (g_max_slots > XHCI_MAX_SLOTS) g_max_slots = XHCI_MAX_SLOTS;
    g_max_ports = (uint8_t)((hcs1 >> 24) & 0xFFU);
    if (g_max_ports > XHCI_MAX_PORTS) g_max_ports = XHCI_MAX_PORTS;

    g_op = (volatile xhci_op_regs_t *)(cap_base + caplen);
    g_rt = (volatile xhci_rt_regs_t *)(cap_base + rtsoff);
    g_db = (volatile uint32_t *)(cap_base + dboff);

    uintptr_t op_base = (uintptr_t)g_op;
    uintptr_t rt_base = (uintptr_t)g_rt;

    // Halt and reset controller
    xhci_w32(op_base + offsetof(xhci_op_regs_t, usbcmd),
             xhci_r32(op_base + offsetof(xhci_op_regs_t, usbcmd)) & ~XHCI_CMD_RUN);
    if (wait_mmio_set(op_base + offsetof(xhci_op_regs_t, usbsts), XHCI_STS_HCH) != 0) {
        sprintf("[xHCI] Timeout halting controller.\n");
        return -1;
    }

    xhci_w32(op_base + offsetof(xhci_op_regs_t, usbcmd),
             xhci_r32(op_base + offsetof(xhci_op_regs_t, usbcmd)) | XHCI_CMD_HCRST);
    if (wait_mmio_clear(op_base + offsetof(xhci_op_regs_t, usbcmd), XHCI_CMD_HCRST) != 0 ||
        wait_mmio_clear(op_base + offsetof(xhci_op_regs_t, usbsts), XHCI_STS_CNR) != 0) {
        sprintf("[xHCI] Timeout resetting controller.\n");
        return -1;
    }

    // configure controller
    g_dcbaa_phys_tbl = (uint64_t *)alloc_zero_page(&g_dcbaa_phys);
    if (!g_dcbaa_phys_tbl || ring_init(&g_cmd_ring) != 0) {
        return -1;
    }

    g_ev_ring = (xhci_trb_t *)alloc_zero_page(&g_ev_ring_phys);
    xhci_erst_entry_t *erst = (xhci_erst_entry_t *)alloc_zero_page(&g_erst_phys);
    if (!g_ev_ring || !erst) {
        return -1;
    }

    erst[0].ring_base_phys = g_ev_ring_phys;
    erst[0].ring_size = XHCI_RING_TRBS;
    g_ev_dequeue = 0;
    g_ev_cycle = 1;

    xhci_w32(op_base + offsetof(xhci_op_regs_t, config), g_max_slots);
    xhci_w64(op_base + offsetof(xhci_op_regs_t, dcbaap), g_dcbaa_phys);
    xhci_w64(op_base + offsetof(xhci_op_regs_t, crcr), g_cmd_ring.phys | 1ULL);

    xhci_w32(rt_base + offsetof(xhci_rt_regs_t, ir[0].erstsz), 1);
    xhci_w64(rt_base + offsetof(xhci_rt_regs_t, ir[0].erstba), g_erst_phys);
    update_erdp();
    xhci_w32(rt_base + offsetof(xhci_rt_regs_t, ir[0].iman),
             xhci_r32(rt_base + offsetof(xhci_rt_regs_t, ir[0].iman)) | (1U << 1));

    xhci_w32(op_base + offsetof(xhci_op_regs_t, usbcmd),
             xhci_r32(op_base + offsetof(xhci_op_regs_t, usbcmd)) | XHCI_CMD_RUN | XHCI_CMD_INTE);
    if (wait_mmio_clear(op_base + offsetof(xhci_op_regs_t, usbsts), XHCI_STS_HCH) != 0) {
        sprintf("[xHCI] Timeout starting controller.\n");
        return -1;
    }

    // Reset connected ports and bind the first USB keyboard found
    for (uint8_t p = 0; p < g_max_ports; p++) {
        uintptr_t p_addr = xhci_portsc_addr(p);
        uint32_t val = xhci_r32(p_addr);

        if (!(val & XHCI_PORTSC_PP)) {
            xhci_w32(p_addr, (val & ~XHCI_PORTSC_W1C_MASK) | XHCI_PORTSC_PP);
            val = xhci_r32(p_addr);
        }

        if (val & XHCI_PORTSC_CCS) {
            xhci_w32(p_addr, (val & ~XHCI_PORTSC_W1C_MASK) | XHCI_PORTSC_PR);
            wait_mmio_set(p_addr, XHCI_PORTSC_PRC);
            val = xhci_r32(p_addr);
            xhci_w32(p_addr, (val & ~XHCI_PORTSC_W1C_MASK) | XHCI_PORTSC_PRC | XHCI_PORTSC_CSC | XHCI_PORTSC_PEC);

            if ((xhci_r32(p_addr) & XHCI_PORTSC_PED) && setup_usb_keyboard(p) == 0) {
                sprintf("[xHCI] Keyboard active on port %u (slot %u).\n", p + 1, g_kbd_slot);
                return 0;
            }
        }
    }

    sprintf("[xHCI] Controller enabled, no keyboard attached.\n");
    return 0;
}