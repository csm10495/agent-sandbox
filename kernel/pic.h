#pragma once
#include "types.h"

/* 8259A PIC I/O ports */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

/* IRQs are remapped to IDT vectors 0x20..0x2F */
#define IRQ_BASE  0x20

void pic_init(void);
void pic_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
