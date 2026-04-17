//! Kernel panic handler.

use core::fmt::Write;
use core::panic::PanicInfo;
use core::sync::atomic::{AtomicBool, Ordering};

use crate::arch::x86_64::{context, serial};

static IN_PANIC: AtomicBool = AtomicBool::new(false);

#[panic_handler]
fn on_panic(info: &PanicInfo) -> ! {
    // Avoid re-entering on nested panic.
    if IN_PANIC.swap(true, Ordering::SeqCst) {
        loop {
            context::disable_interrupts();
            context::halt();
        }
    }

    // Print directly via the serial mutex; bypass the higher-level console.
    let mut s = serial::SERIAL.lock();
    let _ = write!(s, "\n\n*** KERNEL PANIC ***\n{}\n", info);

    // If the `isa-debug-exit` harness is attached, exit QEMU with code 0x11
    // (which maps to exit code 0x23 via the (code<<1)|1 convention) so tests
    // fail fast instead of hanging.
    unsafe {
        use crate::arch::x86_64::port::outl;
        outl(0xF4, 0x11);
    }

    loop {
        context::disable_interrupts();
        context::halt();
    }
}
