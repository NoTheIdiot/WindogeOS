#include <stdint.h>
#include <string.h>
#include <system.h>
#include <stddef.h>
#include <limine.h>
#include <dogeio.h>

extern volatile uint32_t ticks;

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;
static uintptr_t lapic_base;

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define IA32_APIC_BASE_MSR_X2APIC 0x400
#define IA32_APIC_BASE_ADDRESS_MASK 0x0000000FFFFFF000ULL
#define LAPIC_VIRTUAL_BASE 0xFFFFC00000000000ULL
#define APIC_REGISTER_MASKED 0x10000U
#define APIC_SPURIOUS_VECTOR 0xFFU
#define PIC_MASTER_COMMAND_PORT 0x20
#define MASTER_PIC_DATA_PORT 0x21
#define PIC_SLAVE_COMMAND_PORT 0xA0
#define SLAVE_PIC_DATA_PORT 0xA1
#define CPUID_FEAT_EDX_APIC (1U << 9)

#define APIC_REG_TPR         0x0080U
#define APIC_REG_EOI         0x00B0U
#define APIC_REG_SVR         0x00F0U
#define APIC_REG_LVT_TIMER   0x0320U
#define APIC_REG_LVT_THERMAL 0x0330U
#define APIC_REG_LVT_PERF    0x0340U
#define APIC_REG_LVT_LINT0   0x0350U
#define APIC_REG_LVT_LINT1   0x0360U
#define APIC_REG_LVT_ERR     0x0370U
#define APIC_SVR_SOFTWARE_ENABLE 0x100U

#define APIC_REG_TIMER_INITCNT 0x0380U
#define APIC_REG_TIMER_CURRCNT 0x0390U
#define APIC_REG_TIMER_DIV     0x03E0U
#define APIC_TIMER_PERIODIC    (1U << 17)

void cpu_set_apic_base(uintptr_t apic) {
    uint64_t msr_val = rdmsr(IA32_APIC_BASE_MSR);

    msr_val &= ~IA32_APIC_BASE_ADDRESS_MASK;
    msr_val |= ((uint64_t)apic & IA32_APIC_BASE_ADDRESS_MASK) |
               IA32_APIC_BASE_MSR_ENABLE;

    wrmsr(IA32_APIC_BASE_MSR, msr_val);
}

uintptr_t cpu_get_apic_base(void) {
    uint64_t msr_val = rdmsr(IA32_APIC_BASE_MSR);
    return (uintptr_t)(msr_val & IA32_APIC_BASE_ADDRESS_MASK);
}

static inline void write_reg(uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(lapic_base + offset) = val;
}

bool check_apic() {
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

void init_apic(void) {
    if (!check_apic() || hhdm_request.response == NULL) {
        return;
    }

    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    if (apic_base_msr & IA32_APIC_BASE_MSR_X2APIC) {
        return;
    }

    uintptr_t phys_base = cpu_get_apic_base();
    lapic_base = map_kernel_mmio_page((uintptr_t)LAPIC_VIRTUAL_BASE, (uint64_t)phys_base);

    if (lapic_base == 0) {
        return;
    }

    cpu_set_apic_base(phys_base);

    write_reg(APIC_REG_TPR, 0);

    write_reg(APIC_REG_LVT_TIMER, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_THERMAL, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_PERF, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_ERR, APIC_REGISTER_MASKED);

    write_reg(APIC_REG_LVT_LINT0, APIC_REGISTER_MASKED); 
    write_reg(APIC_REG_LVT_LINT1, APIC_REGISTER_MASKED); 

    write_reg(APIC_REG_SVR, APIC_SPURIOUS_VECTOR | APIC_SVR_SOFTWARE_ENABLE);
}

void apic_eoi(void) {
    if (lapic_base != 0) {
        write_reg(APIC_REG_EOI, 0);
    }
}

void init_apic_timer(uint8_t vector, uint32_t initial_count) {
    if (lapic_base == 0) return;

    write_reg(APIC_REG_TIMER_DIV, 0x3);
    write_reg(APIC_REG_LVT_TIMER, (uint32_t)vector | APIC_TIMER_PERIODIC);
    write_reg(APIC_REG_TIMER_INITCNT, initial_count);
}


// the other stuff, like sleep.
void sleep_ms(uint32_t ms) {
    uint32_t start = ticks;
    while ((uint32_t)(ticks - start) < ms) {
        __asm__ volatile("hlt");
    }
}

