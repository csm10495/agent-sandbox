#include "pit.h"
#include "pic.h"

static volatile uint64_t g_ticks = 0;
static uint32_t g_hz = 100;

/* Called from IRQ0 handler */
void pit_tick(void) {
    g_ticks++;
}

uint64_t pit_ticks(void) { return g_ticks; }

void pit_init(uint32_t hz) {
    g_hz = hz;
    uint32_t divisor = (uint32_t)(PIT_BASE_HZ / hz);
    /* Channel 0, mode 3 (square wave), binary */
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)(divisor >> 8));
    pic_unmask(0);   /* unmask IRQ0 */
}

void pit_sleep_ms(uint32_t ms) {
    uint64_t target = g_ticks + ((uint64_t)ms * g_hz) / 1000 + 1;
    while (g_ticks < target)
        cpu_pause();
}
