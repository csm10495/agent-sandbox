#pragma once
#include "types.h"

/* 8254 Programmable Interval Timer */
#define PIT_CH0     0x40
#define PIT_CMD     0x43
#define PIT_BASE_HZ 1193182UL

void     pit_init(uint32_t hz);
void     pit_tick(void);       /* called from IRQ0 handler */
uint64_t pit_ticks(void);
void     pit_sleep_ms(uint32_t ms);
