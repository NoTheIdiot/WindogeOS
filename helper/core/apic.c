/* stolen from osdev.org */

#include <stdint.h>
#include <bool.h>
#include <start.h>
#include <basicutil.h>
#include <stddef.h>
#include <boot/limine.h>
#include <dogeio.h>

extern volatile struct limine_hhdm_request hhdm_request;
static uintptr_t lapic_base;

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define MASTER_PIC_DATA_PORT 0x21
#define SLAVE_PIC_DATA_PORT  0xA1

void cpu_set_apic_base(uintptr_t apic) {
    uint64_t msr_val = rdmsr(IA32_APIC_BASE_MSR);
    
    msr_val &= 0xFFFULL;
    msr_val |= (apic & 0xFFFFF000ULL);
    msr_val |= IA32_APIC_BASE_MSR_ENABLE;

    wrmsr(IA32_APIC_BASE_MSR, msr_val);
}

uintptr_t cpu_get_apic_base(void) {
    uint64_t msr_val = rdmsr(IA32_APIC_BASE_MSR);

#ifdef __PHYSICAL_MEMORY_EXTENSION__
    return (uintptr_t)(msr_val & 0xFFFFFFFFF000ULL);
#else
    return (uintptr_t)(msr_val & 0xFFFFF000UL);
#endif
}

static inline void write_reg(uint32_t offset, uint32_t val)
{
    *(volatile uint32_t *)(lapic_base + offset) = val;
}

static inline uint32_t read_reg(uint32_t offset)
{
    return *(volatile uint32_t *)(lapic_base + offset);
}

/** returns a 'true' value if the CPU supports APIC
 *  and if the local APIC hasn't been disabled in MSRs
 *  note that this requires CPUID to be supported.
 */
bool check_apic() {
   uint32_t eax, edx;
   cpuid(1, &eax, &edx);
   return edx & CPUID_FEAT_EDX_APIC;
}

void enable_apic() {
    /* Section 11.4.1 of 3rd volume of Intel SDM recommends mapping the base address page as strong uncacheable for correct APIC operation. */
    cpu_set_apic_base(cpu_get_apic_base());
    write_reg(0xF0, read_reg(0xF0) | 0x100);
}

static inline void mute_legacy_pic(void) {
    ports_outb(MASTER_PIC_DATA_PORT, 0xFF);
    ports_outb(SLAVE_PIC_DATA_PORT,  0xFF);
}

void init_apic(void)
{
    mute_legacy_pic();

    if (!check_apic())
        return;

    uint64_t hhdm = hhdm_request.response->offset;

    uintptr_t phys_base = cpu_get_apic_base();

    lapic_base = phys_base + hhdm;

    cpu_set_apic_base(phys_base);

    /* Spurious Interrupt Vector Register */
    write_reg(0xF0, 0x1FF);

    /* Divide by 16 */
    write_reg(0x3E0, 0x03);

    /* Timer, vector 32, periodic mode */
    write_reg(0x320, 0x20 | (1 << 17));

    /* Initial count */
    write_reg(0x380, 1000000);
}

void apic_eoi(void)
{
    write_reg(0xB0, 0);
}