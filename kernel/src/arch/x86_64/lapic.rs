//! Local APIC (xAPIC MMIO) driver.
//!
//! We enable the LAPIC on each CPU, register a spurious-interrupt vector, and
//! configure the APIC timer in periodic mode to fire scheduler ticks. No PIT
//! calibration is performed; we pick a divide/count that produces a reasonable
//! tick rate under QEMU (and real hardware) — fine for a hobby kernel where
//! precise timing isn't required.

use core::ptr::{read_volatile, write_volatile};
use core::sync::atomic::{AtomicU64, Ordering};

use x86_64::registers::model_specific::Msr;

use super::idt::VECTOR_TIMER;

const LAPIC_ID: usize = 0x020;
const LAPIC_EOI: usize = 0x0B0;
const LAPIC_SIVR: usize = 0x0F0;
const LAPIC_LVT_TIMER: usize = 0x320;
const LAPIC_TIMER_ICR: usize = 0x380;
const LAPIC_TIMER_DCR: usize = 0x3E0;

/// MSR IA32_APIC_BASE
const IA32_APIC_BASE_MSR: u32 = 0x1B;

/// Virtual address at which the LAPIC MMIO page is accessible.
/// We set this once from the BSP after Limine gives us HHDM.
static LAPIC_VADDR: AtomicU64 = AtomicU64::new(0);

pub fn set_base_vaddr(vaddr: u64) {
    LAPIC_VADDR.store(vaddr, Ordering::SeqCst);
}

fn base() -> *mut u32 {
    LAPIC_VADDR.load(Ordering::Relaxed) as *mut u32
}

fn read(off: usize) -> u32 {
    unsafe { read_volatile(base().add(off / 4)) }
}

fn write(off: usize, v: u32) {
    unsafe { write_volatile(base().add(off / 4), v) }
}

/// Enable the LAPIC on the current CPU.
pub fn init_this_cpu() {
    unsafe {
        // Ensure IA32_APIC_BASE has the global enable bit (bit 11). On modern
        // CPUs it is typically already set, but we assert it for safety. We do
        // NOT move the base address.
        let mut msr = Msr::new(IA32_APIC_BASE_MSR);
        let val = msr.read();
        if val & (1 << 11) == 0 {
            msr.write(val | (1 << 11));
        }
    }
    // Program Spurious-Interrupt Vector register: enable + vector 0xFF.
    write(LAPIC_SIVR, 0x1FF);
}

pub fn id() -> u32 {
    read(LAPIC_ID) >> 24
}

pub fn eoi() {
    write(LAPIC_EOI, 0);
}

/// Configure periodic timer. `initial_count` is counted down at bus-freq/16.
pub fn start_periodic_timer(initial_count: u32) {
    // Divide by 16.
    write(LAPIC_TIMER_DCR, 0b0011);
    // LVT: periodic (bit 17 set) | vector.
    write(LAPIC_LVT_TIMER, (1 << 17) | VECTOR_TIMER as u32);
    // Initial count — starts the timer.
    write(LAPIC_TIMER_ICR, initial_count);
}
