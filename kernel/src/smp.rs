//! SMP bring-up using Limine's MP request.
//!
//! Limine starts each AP in 64-bit long mode, on its own stack, and has them
//! spin on a `goto_address` atomic. Writing a function pointer there releases
//! the AP to execute it. We point all APs at [`ap_start`].

use core::sync::atomic::{AtomicUsize, Ordering};

use limine::mp::Cpu;

use crate::arch::x86_64::{context, gdt, idt, lapic};

pub static APS_ONLINE: AtomicUsize = AtomicUsize::new(0);

/// Entry point for each AP. Limine has already placed us in 64-bit mode with
/// a valid stack. We install per-CPU GDT/IDT, enable the LAPIC, start our
/// local timer, and enter the scheduler.
pub unsafe extern "C" fn ap_start(cpu: &Cpu) -> ! {
    // Register this CPU with the scheduler. Lapic ID from Limine is available
    // directly.
    let _gdt = Box::leak(alloc::boxed::Box::new(gdt::init_cpu()));

    // Build/load a per-CPU IDT. Idt needs a &'static reference that outlives
    // the CPU — we leak it intentionally.
    let ap_idt = Box::leak(alloc::boxed::Box::new(idt::build_idt()));
    idt::load(ap_idt);

    lapic::init_this_cpu();

    // Register with the scheduler and enter.
    let idx = crate::sched::register_cpu(cpu.lapic_id);
    crate::println!(
        "[cpu{}] AP online (lapic_id={}, acpi_id={})",
        idx,
        cpu.lapic_id,
        cpu.id
    );
    APS_ONLINE.fetch_add(1, Ordering::SeqCst);

    // Start LAPIC timer preemption.
    context::enable_interrupts();
    lapic::start_periodic_timer(crate::TIMER_INITIAL_COUNT);

    // Enter scheduler — never returns.
    crate::sched::enter(idx)
}

use alloc::boxed::Box;
