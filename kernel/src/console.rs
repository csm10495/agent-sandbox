//! Console: serial + a pluggable set of on-screen terminal backends.
//!
//! Kernel output is a byte stream that goes to every registered sink:
//!
//!  - **Serial (COM1)**: passthrough — the host's terminal emulator handles
//!    ANSI escapes.
//!  - **On-screen terminals**: one or more [`TerminalSink`] backends driven
//!    by our in-kernel [`AnsiParser`], so they render the same byte stream
//!    (including ANSI colors and clears) equivalently to a serial terminal.
//!
//! Adding a new on-screen backend is a matter of implementing
//! [`TerminalSink`] and registering it here. The parser + dispatching logic
//! is shared and backend-agnostic.

use core::fmt::{self, Write};
use core::sync::atomic::{AtomicBool, Ordering};

use spin::Mutex;

use crate::arch::x86_64::serial;
use crate::fb::Framebuffer;
use crate::term::{AnsiParser, TerminalSink};
use crate::vga_text::VgaText;

/// A video terminal: an owned [`TerminalSink`] plus the parser state that
/// drives it. One of these per physical display backend.
struct VideoTerm<T: TerminalSink + Send> {
    sink: T,
    parser: AnsiParser,
}

impl<T: TerminalSink + Send> VideoTerm<T> {
    const fn new(sink: T) -> Self {
        Self {
            sink,
            parser: AnsiParser::new(),
        }
    }
    fn feed(&mut self, bytes: &[u8]) {
        self.parser.feed(&mut self.sink, bytes);
    }
}

static FB_TERM: Mutex<Option<VideoTerm<Framebuffer>>> = Mutex::new(None);
static VGA_TERM: Mutex<Option<VideoTerm<VgaText>>> = Mutex::new(None);
static SERIAL_READY: AtomicBool = AtomicBool::new(false);

pub fn init_serial() {
    serial::init();
    SERIAL_READY.store(true, Ordering::SeqCst);
}

/// Install a pixel framebuffer backend.
pub fn init_framebuffer(fb: Framebuffer) {
    *FB_TERM.lock() = Some(VideoTerm::new(fb));
}

/// Install the legacy VGA 80x25 text-mode backend. Programs the VGA
/// hardware into BIOS mode 3 and re-uploads the font glyphs, so text
/// actually scans out even if the firmware left the card in a graphics
/// mode.
///
/// SAFETY: `hhdm_offset` must be a valid HHDM offset for the kernel's
/// lifetime.
pub unsafe fn init_vga_text(hhdm_offset: u64) {
    crate::vga_text::init_hardware(hhdm_offset);
    *VGA_TERM.lock() = Some(VideoTerm::new(VgaText::new(hhdm_offset)));
}

pub fn has_video_out() -> bool {
    FB_TERM.lock().is_some() || VGA_TERM.lock().is_some()
}

pub fn video_kind() -> &'static str {
    if FB_TERM.lock().is_some() {
        "framebuffer"
    } else if VGA_TERM.lock().is_some() {
        "vga-text"
    } else {
        "none"
    }
}

pub fn write_str(s: &str) {
    if SERIAL_READY.load(Ordering::Relaxed) {
        serial::write_str(s);
    }
    let bytes = s.as_bytes();
    if let Some(t) = FB_TERM.lock().as_mut() {
        t.feed(bytes);
    }
    if let Some(t) = VGA_TERM.lock().as_mut() {
        t.feed(bytes);
    }
}

pub struct ConsoleWriter;

impl Write for ConsoleWriter {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        write_str(s);
        Ok(())
    }
}

#[doc(hidden)]
pub fn _print(args: fmt::Arguments<'_>) {
    let _ = ConsoleWriter.write_fmt(args);
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => ($crate::console::_print(core::format_args!($($arg)*)));
}

#[macro_export]
macro_rules! println {
    () => ($crate::print!("\n"));
    ($($arg:tt)*) => ($crate::print!("{}\n", core::format_args!($($arg)*)));
}
