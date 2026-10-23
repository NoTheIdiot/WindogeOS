#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <system.h>
#include <basicutil.h>
#include <boot/kernel.h>

#define PIT_CHANNEL_0   0x40
#define PIT_MODE_CMD    0x43
#define PIT_BASE_FREQ   1193182

volatile uint64_t system_ticks = 0;
static uint32_t pit_frequency = 0;

void init_pit(uint32_t target_frequency) {
    if (target_frequency == 0) target_frequency = 1;
    
    uint32_t divisor = PIT_BASE_FREQ / target_frequency;
    
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor == 0) divisor = 1;

    pit_frequency = PIT_BASE_FREQ / divisor;

    ports_outb(PIT_MODE_CMD, 0x36);
    ports_outb(PIT_CHANNEL_0, (uint8_t)(divisor & 0xFF));
    ports_outb(PIT_CHANNEL_0, (uint8_t)((divisor >> 8) & 0xFF));
}

void pit_handler(void) {
    system_ticks++;
    ports_outb(0x20, 0x20);
}

void sleep_ms(uint64_t milliseconds) {
    uint64_t ticks_to_wait = (milliseconds * pit_frequency) / 1000;
    uint64_t target_ticks = system_ticks + ticks_to_wait;
    
    while (system_ticks < target_ticks) {
        asm volatile ("sti; hlt"); 
    }
}
