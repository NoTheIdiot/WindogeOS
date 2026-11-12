#include <dogeio.h>
#include <system.h>
#include <stdint.h>

extern uint32_t ticks;

#define PIT_BASE_FREQ 1193182U
#define PIT_CMD_PORT  0x43
#define PIT_CH2_PORT  0x42
#define PCSPK_PORT    0x61

void speaker_play(uint32_t hz) {
    if (hz == 0) {
        speaker_stop();
        return;
    }

    uint32_t divisor = PIT_BASE_FREQ / hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1) divisor = 1;

    outb(PIT_CMD_PORT, 0xB6);
    outb(PIT_CH2_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH2_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t status = inb(PCSPK_PORT);
    if ((status & 0x03) != 0x03) {
        outb(PCSPK_PORT, status | 0x03);
    }
}

void speaker_stop(void) {
    uint8_t status = inb(PCSPK_PORT) & 0xFC;
    outb(PCSPK_PORT, status);
}

void speaker_beep(uint32_t hz, uint32_t duration_ms) {
    speaker_play(hz);
    sleep_ms(duration_ms);
    speaker_stop();
}