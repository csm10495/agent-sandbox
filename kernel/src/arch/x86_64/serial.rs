//! 16550 UART (COM1) serial driver — used for the primary kernel log and as an
//! I/O channel for automated tests.
//!
//! QEMU maps its first serial device to stdio. VirtualBox can also route COM1
//! to a file or named pipe. The driver is poll-based (no IRQs) to keep the
//! implementation simple and independent of the interrupt controller.

use core::fmt;
use spin::Mutex;

use super::port::{inb, outb};

const COM1: u16 = 0x3F8;

pub struct Serial {
    base: u16,
    initialized: bool,
}

impl Serial {
    const fn new(base: u16) -> Self {
        Self { base, initialized: false }
    }

    pub fn init(&mut self) {
        if self.initialized {
            return;
        }
        unsafe {
            outb(self.base + 1, 0x00); // disable interrupts
            outb(self.base + 3, 0x80); // DLAB on
            outb(self.base + 0, 0x03); // divisor = 3 (38400 baud)
            outb(self.base + 1, 0x00);
            outb(self.base + 3, 0x03); // 8n1
            outb(self.base + 2, 0xC7); // enable FIFO, clear, 14-byte threshold
            outb(self.base + 4, 0x0B); // IRQs enabled, RTS/DSR set
        }
        self.initialized = true;
    }

    fn tx_empty(&self) -> bool {
        unsafe { inb(self.base + 5) & 0x20 != 0 }
    }

    pub fn write_byte(&self, b: u8) {
        while !self.tx_empty() {
            core::hint::spin_loop();
        }
        unsafe {
            outb(self.base, b);
        }
    }

    pub fn write_bytes(&self, bytes: &[u8]) {
        for &b in bytes {
            if b == b'\n' {
                self.write_byte(b'\r');
            }
            self.write_byte(b);
        }
    }

    fn rx_ready(&self) -> bool {
        unsafe { inb(self.base + 5) & 0x01 != 0 }
    }

    /// Non-blocking read. Returns `None` if no byte is available.
    pub fn try_read_byte(&self) -> Option<u8> {
        if self.rx_ready() {
            Some(unsafe { inb(self.base) })
        } else {
            None
        }
    }
}

impl fmt::Write for Serial {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.write_bytes(s.as_bytes());
        Ok(())
    }
}

pub static SERIAL: Mutex<Serial> = Mutex::new(Serial::new(COM1));

pub fn init() {
    SERIAL.lock().init();
}

pub fn write_str(s: &str) {
    SERIAL.lock().write_bytes(s.as_bytes());
}

pub fn try_read_byte() -> Option<u8> {
    SERIAL.lock().try_read_byte()
}
