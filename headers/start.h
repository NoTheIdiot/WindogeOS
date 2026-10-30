#ifndef START_H
#define START_H

#include <stdint.h>

#define APIC_REG_ID                0x0020
#define APIC_REG_VERSION           0x0030
#define APIC_REG_TPR               0x0080
#define APIC_REG_APR               0x0090
#define APIC_REG_PPR               0x00A0
#define APIC_REG_EOI               0x00B0
#define APIC_REG_RRD               0x00C0
#define APIC_REG_LDR               0x00D0
#define APIC_REG_DFR               0x00E0
#define APIC_REG_SVR               0x00F0
#define APIC_REG_ISR_BASE          0x0100
#define APIC_REG_TMR_BASE          0x0180
#define APIC_REG_IRR_BASE          0x0200
#define APIC_REG_ERR               0x0280
#define APIC_REG_ICR_LOW           0x0300
#define APIC_REG_ICR_HIGH          0x0310
#define APIC_REG_LVT_TIMER         0x0320
#define APIC_REG_LVT_THERMAL       0x0330
#define APIC_REG_LVT_PERF          0x0340
#define APIC_REG_LVT_LINT0         0x0350
#define APIC_REG_LVT_LINT1         0x0360
#define APIC_REG_LVT_ERR           0x0370
#define APIC_REG_INITIAL_COUNT     0x0380
#define APIC_REG_CURRENT_COUNT     0x0390
#define APIC_REG_DIVIDE_CONFIG     0x03E0

#define APIC_SVR_SOFTWARE_ENABLE   0x0100
#define APIC_SVR_FOCUS_DISABLE     0x0200

#define APIC_LVT_MODE_ONESHOT      0x00000
#define APIC_LVT_MODE_PERIODIC     0x20000
#define APIC_LVT_MODE_TSC_DEADLINE 0x40000
#define APIC_LVT_INT_MASKED        0x10000

#define CPUID_FEAT_EDX_FPU         (1 << 0)
#define CPUID_FEAT_EDX_VME         (1 << 1)
#define CPUID_FEAT_EDX_DE          (1 << 2)
#define CPUID_FEAT_EDX_PSE         (1 << 3)
#define CPUID_FEAT_EDX_TSC         (1 << 4)
#define CPUID_FEAT_EDX_MSR         (1 << 5)
#define CPUID_FEAT_EDX_PAE         (1 << 6)
#define CPUID_FEAT_EDX_MCE         (1 << 7)
#define CPUID_FEAT_EDX_CX8         (1 << 8)
#define CPUID_FEAT_EDX_APIC        (1 << 9)

#define APIC_DIVIDE_BY_1           0x0B
#define APIC_DIVIDE_BY_2           0x00
#define APIC_DIVIDE_BY_4           0x01
#define APIC_DIVIDE_BY_8           0x02
#define APIC_DIVIDE_BY_16          0x03
#define APIC_DIVIDE_BY_32          0x08
#define APIC_DIVIDE_BY_64          0x09
#define APIC_DIVIDE_BY_128         0x0A

void start_shell(void);
void start_setup(void);

void init_gdt_tss(void);
void init_idt(void);
void init_syscalls(void);
void init_apic(void);


#endif