//! SandboxOS kernel — entry point, early init, BSP + AP bring-up.

#![no_std]
#![no_main]
#![feature(abi_x86_interrupt)]
#![allow(clippy::missing_safety_doc)]

extern crate alloc;

pub mod arch;
pub mod boot;
pub mod console;
pub mod fb;
pub mod fs;
pub mod mem;
pub mod panic;
pub mod sched;
pub mod shell;
pub mod smp;
pub mod term;
pub mod vga_text;

use core::sync::atomic::Ordering;

use alloc::boxed::Box;

use crate::arch::x86_64::{context, gdt, idt, lapic, pic};

/// Initial count loaded into the LAPIC timer. On QEMU TCG this produces a
/// reasonable preemption rate; real hardware will differ but still function.
pub const TIMER_INITIAL_COUNT: u32 = 10_000_000;

/// Physical MMIO base address of the xAPIC registers.
const LAPIC_MMIO_PHYS: u64 = 0xFEE0_0000;

#[no_mangle]
pub extern "C" fn _start() -> ! {
    console::init_serial();
    println!("\n[boot] SandboxOS starting");

    if !boot::BASE_REVISION.is_supported() {
        panic!("limine: unsupported base revision");
    }
    let hhdm = boot::HHDM_REQUEST
        .get_response()
        .expect("limine: no HHDM response");
    mem::pmm::set_hhdm(hhdm.offset());
    println!("[boot] HHDM offset: {:#x}", hhdm.offset());

    pic::remap_and_mask();

    let memmap = boot::MEMMAP_REQUEST
        .get_response()
        .expect("limine: no memory map");
    // Heap first (independent of pmm), then frame allocator.
    unsafe { mem::heap::init(memmap) };
    mem::pmm::init(memmap);
    if let Some((total, used, free)) = mem::pmm::stats() {
        println!(
            "[boot] frames: total={} used={} free={}",
            total, used, free
        );
    }
    let (hu, hf) = mem::heap::stats();
    println!(
        "[boot] kernel heap: {} KiB used / {} KiB free",
        hu / 1024,
        hf / 1024
    );

    let _gdt = Box::leak(Box::new(gdt::init_cpu()));
    let idt_ref = Box::leak(Box::new(idt::build_idt()));
    idt::load(idt_ref);
    println!("[boot] GDT + IDT loaded");

    let have_fb = if let Some(fb_resp) = boot::FRAMEBUFFER_REQUEST.get_response() {
        if let Some(fb) = fb_resp.framebuffers().next() {
            let fb_obj = fb::Framebuffer::new(
                fb.addr(),
                fb.width() as usize,
                fb.height() as usize,
                fb.pitch() as usize,
                fb.bpp() as usize,
            );
            console::init_framebuffer(fb_obj);
            println!(
                "[boot] framebuffer: {}x{} @ {} bpp",
                fb.width(),
                fb.height(),
                fb.bpp()
            );
            true
        } else {
            false
        }
    } else {
        false
    };
    if !have_fb {
        // No pixel framebuffer (e.g. Limine `TEXTMODE=yes`). Fall back to the
        // legacy VGA 80x25 color text buffer at physical 0xB8000, accessed
        // via HHDM.
        unsafe { console::init_vga_text(hhdm.offset()) };
        println!("[boot] video: VGA 80x25 text mode");
    }

    lapic::set_base_vaddr(LAPIC_MMIO_PHYS + hhdm.offset());
    lapic::init_this_cpu();
    println!("[boot] BSP LAPIC id = {}", lapic::id());

    let bsp_idx = sched::register_cpu(lapic::id());

    fs::init();
    println!("[boot] ramfs initialized");

    arch::x86_64::keyboard::init();

    if let Some(mp) = boot::MP_REQUEST.get_response() {
        let bsp_id = mp.bsp_lapic_id();
        let cpus = mp.cpus();
        println!(
            "[boot] SMP: {} CPUs reported by firmware (BSP lapic_id={})",
            cpus.len(),
            bsp_id
        );
        for c in cpus {
            if c.lapic_id == bsp_id {
                continue;
            }
            c.goto_address.write(smp::ap_start);
        }
        let expected = cpus.len().saturating_sub(1);
        let mut spins = 0u64;
        while smp::APS_ONLINE.load(Ordering::SeqCst) < expected && spins < 100_000_000 {
            core::hint::spin_loop();
            spins += 1;
        }
        let online = smp::APS_ONLINE.load(Ordering::SeqCst);
        println!("[boot] SMP: {} of {} AP(s) online", online, expected);
    } else {
        println!("[boot] SMP: no MP response (single-core)");
    }

    sched::spawn("shell", shell::run);

    sched::spawn("heartbeat", || loop {
        sched::sleep_ticks(500);
        println!("[heartbeat] uptime={} ticks", sched::total_ticks());
    });

    sched::spawn("watchdog", || loop {
        sched::sleep_ticks(5);
        if shell::is_shutdown_requested() {
            println!("[shutdown] bye.");
            arch::x86_64::qemu_exit(0x10);
        }
        if shell::is_reboot_requested() {
            println!("[reboot] restarting...");
            unsafe {
                let idtr: [u8; 10] = [0; 10];
                core::arch::asm!("lidt [{}]; int3", in(reg) &idtr);
            }
        }
    });

    context::enable_interrupts();
    lapic::start_periodic_timer(TIMER_INITIAL_COUNT);
    println!("[boot] BSP entering scheduler");
    sched::enter(bsp_idx)
}
