#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <dogeio.h>
#include <limine.h>
#include <io.h>

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;

#define PTE_PRESENT                 (1ULL << 0)
#define PTE_WRITABLE                (1ULL << 1)
#define PTE_PWT                     (1ULL << 3)
#define PTE_PCD                     (1ULL << 4)
#define PTE_HUGE                    (1ULL << 7)
#define PTE_ADDR_MASK               0x000FFFFFFFFFF000ULL

#define IA32_APIC_BASE_MSR          0x1B
#define IA32_APIC_BASE_MSR_ENABLE   (1ULL << 11)
#define IA32_APIC_BASE_MSR_X2APIC   (1ULL << 10)
#define IA32_APIC_BASE_ADDRESS_MASK 0x0000000FFFFFF000ULL

#define APIC_REGISTER_MASKED        0x10000U
#define APIC_SPURIOUS_VECTOR        0xFFU

#define PIC_MASTER_COMMAND_PORT     0x20
#define MASTER_PIC_DATA_PORT        0x21
#define PIC_SLAVE_COMMAND_PORT      0xA0
#define SLAVE_PIC_DATA_PORT         0xA1
#define CPUID_FEAT_EDX_APIC         (1U << 9)

#define APIC_REG_TPR                0x0080U
#define APIC_REG_EOI                0x00B0U
#define APIC_REG_SVR                0x00F0U
#define APIC_REG_ESR                0x0280U
#define APIC_REG_LVT_TIMER          0x0320U
#define APIC_REG_LVT_THERMAL        0x0330U
#define APIC_REG_LVT_PERF           0x0340U
#define APIC_REG_LVT_LINT0          0x0350U
#define APIC_REG_LVT_LINT1          0x0360U
#define APIC_REG_LVT_ERR            0x0370U
#define APIC_SVR_SOFTWARE_ENABLE    0x100U

#define APIC_REG_TIMER_INITCNT      0x0380U
#define APIC_REG_TIMER_CURRCNT      0x0390U
#define APIC_REG_TIMER_DIV          0x03E0U
#define APIC_TIMER_PERIODIC         (1U << 17)

#define PIT_CH2_DATA_PORT           0x42
#define PIT_CMD_PORT                0x43
#define PCSPKR_PORT                 0x61
#define PIT_FREQUENCY               1193182U

static uintptr_t lapic_base = 0;
volatile uint64_t ticks = 0;

void cpu_set_apic_base(uintptr_t apic) {
    uint64_t msr_val = rdmsr(IA32_APIC_BASE_MSR);
    msr_val &= ~IA32_APIC_BASE_ADDRESS_MASK;
    msr_val |= ((uint64_t)apic & IA32_APIC_BASE_ADDRESS_MASK) | IA32_APIC_BASE_MSR_ENABLE;
    wrmsr(IA32_APIC_BASE_MSR, msr_val);
}

uintptr_t cpu_get_apic_base(void) {
    uint64_t msr_val = rdmsr(IA32_APIC_BASE_MSR);
    return (uintptr_t)(msr_val & IA32_APIC_BASE_ADDRESS_MASK);
}

static inline void write_reg(uint32_t offset, uint32_t val) {
    mmio_write32(lapic_base + offset, val);
}

static inline uint32_t read_reg(uint32_t offset) {
    return mmio_read32(lapic_base + offset);
}

bool check_apic(void) {
    uint32_t eax, edx;
    cpuid(1, &eax, &edx);
    return (edx & CPUID_FEAT_EDX_APIC) != 0;
}

static void io_wait(void) {
    (void)inb(0x80);
}

void disable_legacy_pic(void) {
    outb(MASTER_PIC_DATA_PORT, 0xFF);
    outb(SLAVE_PIC_DATA_PORT, 0xFF);

    outb(PIC_MASTER_COMMAND_PORT, 0x11);
    io_wait();
    outb(PIC_SLAVE_COMMAND_PORT, 0x11);
    io_wait();
    outb(MASTER_PIC_DATA_PORT, 0x20);
    io_wait();
    outb(SLAVE_PIC_DATA_PORT, 0x28);
    io_wait();
    outb(MASTER_PIC_DATA_PORT, 0x04);
    io_wait();
    outb(SLAVE_PIC_DATA_PORT, 0x02);
    io_wait();
    outb(MASTER_PIC_DATA_PORT, 0x01);
    io_wait();
    outb(SLAVE_PIC_DATA_PORT, 0x01);
    io_wait();

    outb(MASTER_PIC_DATA_PORT, 0xFF);
    outb(SLAVE_PIC_DATA_PORT, 0xFF);
}

bool init_apic(void) {
    if (!check_apic() || hhdm_request.response == NULL) {
        return false;
    }

    disable_legacy_pic();

    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    if (apic_base_msr & IA32_APIC_BASE_MSR_X2APIC) {
        wrmsr(IA32_APIC_BASE_MSR, apic_base_msr & ~(IA32_APIC_BASE_MSR_X2APIC | IA32_APIC_BASE_MSR_ENABLE));
    }

    uintptr_t phys_base = cpu_get_apic_base();
    lapic_base = (uintptr_t)map_kernel_mmio((uint64_t)phys_base, 4096);
    if (lapic_base == 0) {
        return false;
    }

    cpu_set_apic_base(phys_base);

    write_reg(APIC_REG_ESR, 0);
    write_reg(APIC_REG_ESR, 0);

    write_reg(APIC_REG_TPR, 0);

    write_reg(APIC_REG_LVT_TIMER, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_THERMAL, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_PERF, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_ERR, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_LINT0, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_LINT1, APIC_REGISTER_MASKED);

    write_reg(APIC_REG_SVR, APIC_SPURIOUS_VECTOR | APIC_SVR_SOFTWARE_ENABLE);
    return true;
}

void apic_eoi(void) {
    if (lapic_base != 0) {
        write_reg(APIC_REG_EOI, 0);
    }
}

bool init_apic_timer(uint8_t vector, uint32_t ms) {
    if (lapic_base == 0 || vector < 32 || ms == 0) {
        return false;
    }

    write_reg(APIC_REG_TIMER_DIV, 0x3);

    uint8_t spkr = inb(PCSPKR_PORT);
    outb(PCSPKR_PORT, (spkr & ~0x02) | 0x01);

    uint16_t pit_ticks_10ms = PIT_FREQUENCY / 100;
    outb(PIT_CMD_PORT, 0xB0);
    outb(PIT_CH2_DATA_PORT, (uint8_t)(pit_ticks_10ms & 0xFF));
    outb(PIT_CH2_DATA_PORT, (uint8_t)((pit_ticks_10ms >> 8) & 0xFF));

    spkr = inb(PCSPKR_PORT);
    outb(PCSPKR_PORT, spkr & ~0x01);
    outb(PCSPKR_PORT, spkr | 0x01);

    write_reg(APIC_REG_LVT_TIMER, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_TIMER_INITCNT, 0xFFFFFFFFU);

    while ((inb(PCSPKR_PORT) & 0x20) == 0) {
        __asm__ volatile ("pause");
    }

    write_reg(APIC_REG_LVT_TIMER, APIC_REGISTER_MASKED);
    uint32_t elapsed_10ms = 0xFFFFFFFFU - read_reg(APIC_REG_TIMER_CURRCNT);

    outb(PCSPKR_PORT, inb(PCSPKR_PORT) & ~0x01);

    uint32_t ticks_per_ms = elapsed_10ms / 10;
    if (ticks_per_ms == 0) {
        return false;
    }

    write_reg(APIC_REG_TIMER_DIV, 0x3);
    write_reg(APIC_REG_LVT_TIMER, (uint32_t)vector | APIC_TIMER_PERIODIC);
    write_reg(APIC_REG_TIMER_INITCNT, ticks_per_ms * ms);
    return true;
}

void apic_tick(void) {
    ticks++;
    apic_eoi();
}

void sleep_ms(uint64_t ms) {
    if (lapic_base == 0) {
        return;
    }

    __asm__ volatile ("sti");

    uint64_t start = ticks;
    while ((ticks - start) < ms) {
        __asm__ volatile ("hlt");
    }
}
