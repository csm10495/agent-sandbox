pub mod context;
pub mod cpuid;
pub mod gdt;
pub mod idt;
pub mod keyboard;
pub mod lapic;
pub mod pic;
pub mod port;
pub mod serial;

/// Pause the CPU by executing `hlt` with interrupts enabled. Used by idle
/// loops to reduce power consumption until the next IRQ.
#[inline(always)]
pub fn idle_once() {
    unsafe {
        core::arch::asm!("sti; hlt", options(nomem, nostack, preserves_flags));
    }
}

/// Shut down QEMU via the `isa-debug-exit` device. Only works when QEMU is
/// started with `-device isa-debug-exit,iobase=0xf4,iosize=0x04`. Used by the
/// functional test harness. On other platforms this is a no-op.
pub fn qemu_exit(code: u32) -> ! {
    unsafe {
        port::outl(0xF4, code);
    }
    // If isa-debug-exit is not attached, fall back to a power-off attempt.
    loop {
        context::disable_interrupts();
        context::halt();
    }
}
