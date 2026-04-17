//! Kernel heap using `linked_list_allocator`.
//!
//! Bootstrapped directly from Limine's memory map so it doesn't depend on the
//! frame allocator (breaking the chicken-and-egg cycle).

use linked_list_allocator::LockedHeap;
use limine::memory_map::EntryType;
use limine::response::MemoryMapResponse;

use super::pmm::{phys_to_virt, FRAME_SIZE};

#[global_allocator]
pub static ALLOCATOR: LockedHeap = LockedHeap::empty();

pub const INITIAL_HEAP_SIZE: usize = 16 * 1024 * 1024; // 16 MiB

static mut HEAP_PHYS_RANGE: (u64, u64) = (0, 0);

pub fn heap_phys_range() -> (u64, u64) {
    unsafe { HEAP_PHYS_RANGE }
}

/// Initialize the global heap by carving a contiguous region out of the
/// largest usable memory region reported by Limine.
///
/// SAFETY: Must be called exactly once, during early boot.
pub unsafe fn init(memmap: &MemoryMapResponse) {
    let mut best: Option<(u64, u64)> = None;
    for entry in memmap.entries() {
        if entry.entry_type != EntryType::USABLE {
            continue;
        }
        let base = (entry.base + (FRAME_SIZE as u64 - 1)) & !(FRAME_SIZE as u64 - 1);
        if entry.base + entry.length <= base {
            continue;
        }
        let base = base.max(0x100000);
        if base + INITIAL_HEAP_SIZE as u64 > entry.base + entry.length {
            continue;
        }
        let length = entry.base + entry.length - base;
        match best {
            None => best = Some((base, length)),
            Some((_, bl)) if length > bl => best = Some((base, length)),
            _ => {}
        }
    }
    let (base, _) = best.expect("heap init: no usable memory region large enough");
    let size = INITIAL_HEAP_SIZE;
    let va = phys_to_virt(base);
    ALLOCATOR.lock().init(va, size);
    HEAP_PHYS_RANGE = (base, base + size as u64);
}

pub fn stats() -> (usize, usize) {
    let h = ALLOCATOR.lock();
    (h.used(), h.free())
}
