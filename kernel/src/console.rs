//! Console: serial + optional framebuffer text rendering. All kernel output
//! goes through here. `print!`/`println!` macros delegate to [`write_str`].

use core::fmt::{self, Write};
use core::sync::atomic::{AtomicBool, Ordering};

use spin::Mutex;

use crate::arch::x86_64::serial;
use crate::fb::Framebuffer;

static FB: Mutex<Option<Framebuffer>> = Mutex::new(None);
static SERIAL_READY: AtomicBool = AtomicBool::new(false);

pub fn init_serial() {
    serial::init();
    SERIAL_READY.store(true, Ordering::SeqCst);
}

pub fn init_framebuffer(fb: Framebuffer) {
    *FB.lock() = Some(fb);
}

pub fn write_str(s: &str) {
    if SERIAL_READY.load(Ordering::Relaxed) {
        serial::write_str(s);
    }
    if let Some(fb) = FB.lock().as_mut() {
        fb.write_str(s);
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
