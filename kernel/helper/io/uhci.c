#include <system.h>
#include <limine.h>

#define UHCI_USBCMD      0x00
#define UHCI_USBSTS      0x02
#define UHCI_USBINTR     0x04
#define UHCI_FRNUM       0x06
#define UHCI_FRBASEADD   0x08
#define UHCI_SOF         0x0C
#define UHCI_PORTSC1     0x10
#define UHCI_PORTSC2     0x12

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

typedef struct usb_setup_packet {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_packet_t;

static uint16_t g_uhci_io_base = 0;
uint32_t *g_frame_list_virt = NULL;
static uint32_t g_frame_list_phys = 0;
static uint8_t g_assigned_kbd_addr = 1;

extern volatile struct limine_hhdm_request hhdm_request;

bool uhci_find_controller(uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func, uint16_t *out_bar4) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t class_reg = pcie_read32(bus, slot, func, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t subclass = (class_reg >> 16) & 0xFF;
                uint8_t prog_if = (class_reg >> 8) & 0xFF;

                if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x00) {
                    *out_bus = bus;
                    *out_slot = slot;
                    *out_func = func;
                    
                    uint32_t bar4 = pcie_read32(bus, slot, func, 0x20);
                    *out_bar4 = (uint16_t)(bar4 & ~0x1F);
                    return true;
                }
            }
        }
    }
    return false;
}

bool uhci_send_control(uint8_t dev_addr, uint8_t req_type, uint8_t request, uint16_t value, uint16_t index, uint16_t length) {
    uint64_t hhdm_offset = hhdm_request.response->offset;

    uint64_t setup_phys = pmm_alloc_zeroed_page();
    volatile usb_setup_packet_t *setup = (volatile usb_setup_packet_t *)(setup_phys + hhdm_offset);
    setup->bmRequestType = req_type;
    setup->bRequest = request;
    setup->wValue = value;
    setup->wIndex = index;
    setup->wLength = length;

    uint64_t td1_phys = pmm_alloc_zeroed_page();
    uint64_t td2_phys = pmm_alloc_zeroed_page();
    uhci_td_t *td1 = (uhci_td_t *)(td1_phys + hhdm_offset);
    uhci_td_t *td2 = (uhci_td_t *)(td2_phys + hhdm_offset);

    td1->link_pointer = td2_phys;
    td1->control = (1 << 26) | (3 << 27); 
    td1->token = ((sizeof(usb_setup_packet_t) - 1) << 21) | (0 << 19) | (0 << 15) | ((dev_addr & 0x7F) << 8) | 0x2D;
    td1->buffer_pointer = (uint32_t)setup_phys;

    td2->link_pointer = 0x00000001; 
    td2->control = (1 << 26) | (3 << 27);
    td2->token = (0x7FF << 21) | (1 << 19) | (0 << 15) | ((dev_addr & 0x7F) << 8) | 0x69;
    td2->buffer_pointer = 0;

    td1->control |= (1 << 23);
    td2->control |= (1 << 23);

    uint64_t qh_phys = pmm_alloc_zeroed_page();
    uhci_qh_t *qh = (uhci_qh_t *)(qh_phys + hhdm_offset);
    qh->horizontal_link = 0x00000001;
    qh->element_link = td1_phys;

    if (g_frame_list_virt) {
        for (int i = 0; i < 1024; i++) {
            g_frame_list_virt[i] = qh_phys | 0x2;
        }
    }

    uint32_t timeout = 500000;
    while ((td2->control & (1 << 23)) && timeout > 0) {
        timeout--;
    }

    return (timeout > 0 && !(td2->control & 0x7B0000));
}

void uhci_init_driver(void) {
    uint8_t bus, slot, func;
    uint16_t io_base;

    if (!uhci_find_controller(&bus, &slot, &func, &io_base)) {
        return;
    }

    g_uhci_io_base = io_base;

    uint32_t legacy = pcie_read32(bus, slot, func, 0xC0);
    pcie_write32(bus, slot, func, 0xC0, legacy | 0x2000);

    uint32_t pci_cmd = pcie_read32(bus, slot, func, 0x04);
    pcie_write32(bus, slot, func, 0x04, pci_cmd | 0x05);

    uint64_t frame_phys = pmm_alloc_zeroed_page();
    g_frame_list_phys = (uint32_t)frame_phys;
    g_frame_list_virt = (uint32_t *)(frame_phys + hhdm_request.response->offset);

    for (int i = 0; i < 1024; i++) {
        g_frame_list_virt[i] = 0x00000001;
    }

    outw(io_base + UHCI_USBCMD, 0x0002);
    for (volatile int i = 0; i < 10000; i++);
    outw(io_base + UHCI_USBCMD, 0x0000);

    outl(io_base + UHCI_FRBASEADD, g_frame_list_phys);
    outw(io_base + UHCI_FRNUM, 0x0000);
    outw(io_base + UHCI_USBINTR, 0x000F);
    outw(io_base + UHCI_USBCMD, 0x0001 | 0x0080);
}

void uhci_poll_ports(void) {
    if (g_uhci_io_base == 0) return;

    for (int i = 0; i < 2; i++) {
        uint16_t port_reg = UHCI_PORTSC1 + (i * 2);
        uint16_t status = inw(g_uhci_io_base + port_reg);

        if (status & 0x0001) {
            if (status & 0x0002) {
                outw(g_uhci_io_base + port_reg, status | 0x0002);
            }

            uint16_t current_status = inw(g_uhci_io_base + port_reg);
            outw(g_uhci_io_base + port_reg, current_status | 0x0200);

            for (volatile int j = 0; j < 500000; j++);

            current_status = inw(g_uhci_io_base + port_reg);
            outw(g_uhci_io_base + port_reg, (current_status & ~0x0200) | 0x0004);

            for (volatile int j = 0; j < 200000; j++);

            uhci_send_control(0, 0x00, 0x05, 1, 0, 0);
            for (volatile int j = 0; j < 100000; j++);

            uhci_send_control(1, 0x00, 0x09, 1, 0, 0);
        }
    }
}

uint8_t get_assigned_keyboard_address(void) {
    return g_assigned_kbd_addr;
}