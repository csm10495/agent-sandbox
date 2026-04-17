//! Per-CPU IDT with CPU exception handlers and one APIC-timer vector for
//! preemption.

use x86_64::VirtAddr;
use x86_64::registers::control::Cr2;
use x86_64::structures::idt::{InterruptDescriptorTable, InterruptStackFrame, PageFaultErrorCode};

use super::gdt::DOUBLE_FAULT_IST_INDEX;
use super::lapic;

/// First IRQ vector we hand out to devices. 0x20 is the LAPIC timer.
pub const VECTOR_TIMER: u8 = 0x20;
pub const VECTOR_SPURIOUS: u8 = 0xFF;

pub fn build_idt() -> InterruptDescriptorTable {
    let mut idt = InterruptDescriptorTable::new();
    idt.breakpoint.set_handler_fn(breakpoint);
    idt.invalid_opcode.set_handler_fn(invalid_opcode);
    idt.general_protection_fault.set_handler_fn(gp_fault);
    idt.page_fault.set_handler_fn(page_fault);
    idt.divide_error.set_handler_fn(div_error);
    idt.overflow.set_handler_fn(overflow);
    idt.bound_range_exceeded.set_handler_fn(bound_range);
    idt.device_not_available.set_handler_fn(device_na);
    idt.invalid_tss.set_handler_fn(invalid_tss);
    idt.segment_not_present.set_handler_fn(seg_not_present);
    idt.stack_segment_fault.set_handler_fn(stack_seg);
    idt.alignment_check.set_handler_fn(align_check);
    idt.simd_floating_point.set_handler_fn(simd_fp);

    // double_fault and machine_check have `-> !` signatures in the x86_64
    // crate type aliases, but modern rustc rejects `-> !` x86-interrupt
    // function definitions. Install non-diverging wrappers via set_handler_addr.
    unsafe {
        idt.double_fault
            .set_handler_addr(VirtAddr::new(double_fault as u64))
            .set_stack_index(DOUBLE_FAULT_IST_INDEX);
        idt.machine_check
            .set_handler_addr(VirtAddr::new(machine_check as u64));
    }

    idt[VECTOR_TIMER as u8].set_handler_fn(timer_stub);
    idt[VECTOR_SPURIOUS as u8].set_handler_fn(spurious);
    idt
}

pub fn load(idt: &'static InterruptDescriptorTable) {
    idt.load();
}

extern "x86-interrupt" fn breakpoint(frame: InterruptStackFrame) {
    crate::println!("[exc] breakpoint at {:#x}", frame.instruction_pointer.as_u64());
}

extern "x86-interrupt" fn invalid_opcode(frame: InterruptStackFrame) {
    panic!("invalid opcode at {:#x}", frame.instruction_pointer.as_u64());
}

extern "x86-interrupt" fn gp_fault(frame: InterruptStackFrame, err: u64) {
    panic!(
        "#GP(err={:#x}) at {:#x}",
        err,
        frame.instruction_pointer.as_u64()
    );
}

extern "x86-interrupt" fn page_fault(frame: InterruptStackFrame, err: PageFaultErrorCode) {
    let addr = Cr2::read().map(|a| a.as_u64()).unwrap_or(0);
    panic!(
        "#PF(err={:?}) accessing {:#x} at rip={:#x}",
        err,
        addr,
        frame.instruction_pointer.as_u64()
    );
}

extern "x86-interrupt" fn div_error(frame: InterruptStackFrame) {
    panic!("#DE at {:#x}", frame.instruction_pointer.as_u64());
}

extern "x86-interrupt" fn overflow(frame: InterruptStackFrame) {
    panic!("#OF at {:#x}", frame.instruction_pointer.as_u64());
}

extern "x86-interrupt" fn bound_range(frame: InterruptStackFrame) {
    panic!("#BR at {:#x}", frame.instruction_pointer.as_u64());
}

extern "x86-interrupt" fn device_na(frame: InterruptStackFrame) {
    panic!("#NM at {:#x}", frame.instruction_pointer.as_u64());
}

extern "x86-interrupt" fn invalid_tss(_frame: InterruptStackFrame, err: u64) {
    panic!("#TS err={:#x}", err);
}

extern "x86-interrupt" fn seg_not_present(_frame: InterruptStackFrame, err: u64) {
    panic!("#NP err={:#x}", err);
}

extern "x86-interrupt" fn stack_seg(_frame: InterruptStackFrame, err: u64) {
    panic!("#SS err={:#x}", err);
}

extern "x86-interrupt" fn align_check(_frame: InterruptStackFrame, err: u64) {
    panic!("#AC err={:#x}", err);
}

extern "x86-interrupt" fn machine_check(_frame: InterruptStackFrame) {
    panic!("#MC");
}

extern "x86-interrupt" fn simd_fp(frame: InterruptStackFrame) {
    panic!("#XM at {:#x}", frame.instruction_pointer.as_u64());
}

extern "x86-interrupt" fn double_fault(frame: InterruptStackFrame, err: u64) {
    panic!(
        "DOUBLE FAULT err={:#x} at {:#x}",
        err,
        frame.instruction_pointer.as_u64()
    );
}

extern "x86-interrupt" fn timer_stub(_frame: InterruptStackFrame) {
    // Acknowledge LAPIC first.
    lapic::eoi();
    // Call into the scheduler. Scheduler may context-switch; when/if we return
    // from here, `iretq` resumes whichever thread we got back on.
    crate::sched::timer_tick();
}

extern "x86-interrupt" fn spurious(_frame: InterruptStackFrame) {
    // Spurious interrupts do not require EOI.
}
