//! Polled PS/2 keyboard driver — translates scan codes to ASCII for the shell.
//!
//! The driver is polled (not IRQ-driven) to avoid needing an IOAPIC. The shell
//! loop calls [`try_read_char`] each iteration to drain any new keys.
//!
//! Scan codes are PS/2 set 1 (the default from the legacy 8042 controller).

use core::sync::atomic::{AtomicBool, Ordering};

use super::port::{inb, outb};

const PS2_DATA: u16 = 0x60;
const PS2_STATUS: u16 = 0x64;

static SHIFT: AtomicBool = AtomicBool::new(false);
static CAPS: AtomicBool = AtomicBool::new(false);
static EXTENDED: AtomicBool = AtomicBool::new(false);

fn lowercase_map(code: u8) -> Option<char> {
    // PS/2 scan-code set 1 lower half (Make codes). Not exhaustive — covers
    // printable ASCII + Enter + Backspace + Space + Tab.
    Some(match code {
        0x02 => '1', 0x03 => '2', 0x04 => '3', 0x05 => '4', 0x06 => '5',
        0x07 => '6', 0x08 => '7', 0x09 => '8', 0x0A => '9', 0x0B => '0',
        0x0C => '-', 0x0D => '=',
        0x0E => '\u{0008}', // Backspace
        0x0F => '\t',
        0x10 => 'q', 0x11 => 'w', 0x12 => 'e', 0x13 => 'r', 0x14 => 't',
        0x15 => 'y', 0x16 => 'u', 0x17 => 'i', 0x18 => 'o', 0x19 => 'p',
        0x1A => '[', 0x1B => ']',
        0x1C => '\n',
        0x1E => 'a', 0x1F => 's', 0x20 => 'd', 0x21 => 'f', 0x22 => 'g',
        0x23 => 'h', 0x24 => 'j', 0x25 => 'k', 0x26 => 'l',
        0x27 => ';', 0x28 => '\'', 0x29 => '`', 0x2B => '\\',
        0x2C => 'z', 0x2D => 'x', 0x2E => 'c', 0x2F => 'v', 0x30 => 'b',
        0x31 => 'n', 0x32 => 'm',
        0x33 => ',', 0x34 => '.', 0x35 => '/',
        0x39 => ' ',
        _ => return None,
    })
}

fn shifted_map(code: u8) -> Option<char> {
    Some(match code {
        0x02 => '!', 0x03 => '@', 0x04 => '#', 0x05 => '$', 0x06 => '%',
        0x07 => '^', 0x08 => '&', 0x09 => '*', 0x0A => '(', 0x0B => ')',
        0x0C => '_', 0x0D => '+',
        0x1A => '{', 0x1B => '}',
        0x27 => ':', 0x28 => '"', 0x29 => '~', 0x2B => '|',
        0x33 => '<', 0x34 => '>', 0x35 => '?',
        _ => {
            let c = lowercase_map(code)?;
            return Some(if c.is_ascii_alphabetic() {
                c.to_ascii_uppercase()
            } else {
                c
            });
        }
    })
}

pub fn init() {
    // Drain any pending byte.
    while unsafe { inb(PS2_STATUS) } & 1 != 0 {
        let _ = unsafe { inb(PS2_DATA) };
    }
    // Leave the 8042 in whatever state the firmware put it — we only read.
    // Also explicitly disable mouse line so we don't see second-PS/2-port data.
    let _ = unsafe { outb(PS2_STATUS, 0xA7) }; // disable aux
    // Re-drain.
    while unsafe { inb(PS2_STATUS) } & 1 != 0 {
        let _ = unsafe { inb(PS2_DATA) };
    }
}

/// Non-blocking: returns `Some(ch)` if a printable char is available.
pub fn try_read_char() -> Option<char> {
    if unsafe { inb(PS2_STATUS) } & 1 == 0 {
        return None;
    }
    let code = unsafe { inb(PS2_DATA) };

    if code == 0xE0 {
        EXTENDED.store(true, Ordering::Relaxed);
        return None;
    }
    let ext = EXTENDED.swap(false, Ordering::Relaxed);

    let released = code & 0x80 != 0;
    let make = code & 0x7F;

    // Shift
    if make == 0x2A || make == 0x36 {
        SHIFT.store(!released, Ordering::Relaxed);
        return None;
    }
    // Caps-lock (toggle on press)
    if !released && make == 0x3A {
        CAPS.store(!CAPS.load(Ordering::Relaxed), Ordering::Relaxed);
        return None;
    }
    if released {
        return None;
    }
    // Extended arrow keys: Up=0x48, Down=0x50
    if ext {
        return match make {
            0x48 => Some('\x11'), // UP   — private sentinel
            0x50 => Some('\x12'), // DOWN — private sentinel
            _ => None,
        };
    }

    let shifted = SHIFT.load(Ordering::Relaxed);
    let caps = CAPS.load(Ordering::Relaxed);
    let ch = if shifted { shifted_map(make) } else { lowercase_map(make) }?;

    if caps && ch.is_ascii_alphabetic() {
        Some(if shifted { ch.to_ascii_lowercase() } else { ch.to_ascii_uppercase() })
    } else {
        Some(ch)
    }
}
