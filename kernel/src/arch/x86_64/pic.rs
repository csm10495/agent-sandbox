//! Legacy 8259 PIC — we remap its vectors out of the exception range and then
//! mask all IRQs. All device interrupts we actually care about are routed
//! through the LAPIC timer (for scheduling), keeping the interrupt surface
//! tiny.

use super::port::{io_wait, outb};

const PIC1_CMD: u16 = 0x20;
const PIC1_DATA: u16 = 0x21;
const PIC2_CMD: u16 = 0xA0;
const PIC2_DATA: u16 = 0xA1;

/// Remap both PICs to vectors 0x30..0x40 (out of the CPU exception range) and
/// then mask every IRQ.
pub fn remap_and_mask() {
    unsafe {
        // ICW1: start init, cascade mode, ICW4 required.
        outb(PIC1_CMD, 0x11);
        io_wait();
        outb(PIC2_CMD, 0x11);
        io_wait();
        // ICW2: vector offsets.
        outb(PIC1_DATA, 0x30);
        io_wait();
        outb(PIC2_DATA, 0x38);
        io_wait();
        // ICW3: cascade.
        outb(PIC1_DATA, 0x04);
        io_wait();
        outb(PIC2_DATA, 0x02);
        io_wait();
        // ICW4: 8086 mode.
        outb(PIC1_DATA, 0x01);
        io_wait();
        outb(PIC2_DATA, 0x01);
        io_wait();
        // Mask all IRQs.
        outb(PIC1_DATA, 0xFF);
        outb(PIC2_DATA, 0xFF);
    }
}
