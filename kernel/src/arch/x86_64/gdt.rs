//! Per-CPU GDT and TSS.
//!
//! Each CPU gets its own GDT with a unique TSS selector (needed because the
//! CPU's `TR` register points to a TSS; TSSes can't be shared across cores).

use alloc::boxed::Box;
use x86_64::instructions::segmentation::{Segment, CS, DS, ES, FS, GS, SS};
use x86_64::instructions::tables::load_tss;
use x86_64::registers::segmentation::SegmentSelector;
use x86_64::structures::gdt::{Descriptor, GlobalDescriptorTable};
use x86_64::structures::tss::TaskStateSegment;
use x86_64::PrivilegeLevel;

pub const DOUBLE_FAULT_IST_INDEX: u16 = 0;

pub struct CpuGdt {
    pub gdt: &'static GlobalDescriptorTable,
    pub tss: &'static TaskStateSegment,
    pub code_sel: SegmentSelector,
    pub data_sel: SegmentSelector,
    pub tss_sel: SegmentSelector,
}

/// Build and load a new GDT+TSS for the current CPU. Returns a handle with the
/// selectors actually in use. The returned structure is leaked intentionally:
/// CPU structures live for the lifetime of the kernel.
pub fn init_cpu() -> CpuGdt {
    // Allocate a Double-Fault IST stack. 16 KiB is plenty.
    let df_stack = Box::leak(Box::new([0u8; 16 * 1024]));
    let stack_top = x86_64::VirtAddr::from_ptr(df_stack.as_ptr()) + df_stack.len() as u64;

    let mut tss = TaskStateSegment::new();
    tss.interrupt_stack_table[DOUBLE_FAULT_IST_INDEX as usize] = stack_top;

    let tss: &'static mut TaskStateSegment = Box::leak(Box::new(tss));

    let mut gdt = GlobalDescriptorTable::new();
    let code_sel = gdt.append(Descriptor::kernel_code_segment());
    let data_sel = gdt.append(Descriptor::kernel_data_segment());
    let tss_sel = gdt.append(Descriptor::tss_segment(tss));

    let gdt: &'static GlobalDescriptorTable = Box::leak(Box::new(gdt));

    gdt.load();
    unsafe {
        CS::set_reg(code_sel);
        DS::set_reg(SegmentSelector::new(0, PrivilegeLevel::Ring0));
        ES::set_reg(SegmentSelector::new(0, PrivilegeLevel::Ring0));
        FS::set_reg(SegmentSelector::new(0, PrivilegeLevel::Ring0));
        GS::set_reg(SegmentSelector::new(0, PrivilegeLevel::Ring0));
        SS::set_reg(data_sel);
        load_tss(tss_sel);
    }

    CpuGdt { gdt, tss, code_sel, data_sel, tss_sel }
}
