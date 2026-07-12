#include "pic.h"

/* Remap both PIC chips so IRQs 0-15 map to INT vectors 0x20-0x2F */
void pic_init(void) {
    /* Save masks */
    uint8_t m1 = inb(PIC1_DATA);
    uint8_t m2 = inb(PIC2_DATA);

    /* ICW1: start init sequence, cascade mode */
    outb(PIC1_CMD,  0x11); io_wait();
    outb(PIC2_CMD,  0x11); io_wait();
    /* ICW2: vector offsets */
    outb(PIC1_DATA, IRQ_BASE);      io_wait();   /* IRQ0-7  -> INT 32-39 */
    outb(PIC2_DATA, IRQ_BASE + 8);  io_wait();   /* IRQ8-15 -> INT 40-47 */
    /* ICW3: cascade wiring */
    outb(PIC1_DATA, 0x04); io_wait();  /* IR2 is slave */
    outb(PIC2_DATA, 0x02); io_wait();  /* slave ID 2   */
    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    /* Restore masks */
    outb(PIC1_DATA, m1);
    outb(PIC2_DATA, m2);
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = 1 << (irq & 7);
    outb(port, inb(port) | bit);
}

void pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = 1 << (irq & 7);
    outb(port, inb(port) & ~bit);
}
