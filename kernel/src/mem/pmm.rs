//! Physical frame allocator.
//!
//! Built on top of the bitmap allocator from `shared`, covering the usable
//! regions of Limine's memory map. Physical addresses are accessed via the
//! HHDM (Higher Half Direct Map) offset — any physical address P can be
//! read/written at virtual address `P + HHDM_OFFSET`.

use core::sync::atomic::{AtomicU64, Ordering};

use limine::memory_map::EntryType;
use limine::response::MemoryMapResponse;
use shared::bitmap::Bitmap;
use spin::Mutex;

pub const FRAME_SIZE: usize = 4096;

static HHDM_OFFSET: AtomicU64 = AtomicU64::new(0);

pub fn set_hhdm(offset: u64) {
    HHDM_OFFSET.store(offset, Ordering::SeqCst);
}

pub fn hhdm() -> u64 {
    HHDM_OFFSET.load(Ordering::Relaxed)
}

pub fn phys_to_virt(pa: u64) -> *mut u8 {
    (pa + hhdm()) as *mut u8
}

pub struct FrameAllocator {
    bitmap: Bitmap,
    /// Physical address of bit 0.
    base: u64,
}

impl FrameAllocator {
    pub fn new(base: u64, total_frames: usize) -> Self {
        Self { bitmap: Bitmap::new(total_frames), base }
    }

    pub fn total_frames(&self) -> usize {
        self.bitmap.capacity()
    }

    pub fn used_frames(&self) -> usize {
        self.bitmap.used()
    }

    pub fn free_frames(&self) -> usize {
        self.bitmap.free()
    }

    /// Mark a range of physical frames as used (e.g. reserved/bootloader).
    pub fn mark_used_range(&mut self, start_pa: u64, end_pa: u64) {
        let start = start_pa.saturating_sub(self.base) as usize / FRAME_SIZE;
        let end = (end_pa.saturating_sub(self.base) as usize + FRAME_SIZE - 1) / FRAME_SIZE;
        let end = end.min(self.bitmap.capacity());
        for i in start..end {
            self.bitmap.set(i);
        }
    }

    /// Mark a range as free.
    pub fn mark_free_range(&mut self, start_pa: u64, end_pa: u64) {
        let start = start_pa.saturating_sub(self.base) as usize / FRAME_SIZE;
        let end = (end_pa.saturating_sub(self.base) as usize) / FRAME_SIZE;
        let end = end.min(self.bitmap.capacity());
        for i in start..end {
            self.bitmap.clear(i);
        }
    }

    pub fn alloc(&mut self) -> Option<u64> {
        self.bitmap.alloc().map(|i| self.base + (i as u64) * FRAME_SIZE as u64)
    }

    pub fn free(&mut self, pa: u64) {
        let idx = (pa - self.base) as usize / FRAME_SIZE;
        self.bitmap.clear(idx);
    }
}

pub static FRAME_ALLOC: Mutex<Option<FrameAllocator>> = Mutex::new(None);

/// Initialize the frame allocator from a Limine memory map.
pub fn init(memmap: &MemoryMapResponse) {
    let mut max_addr: u64 = 0;
    for entry in memmap.entries() {
        if entry.entry_type == EntryType::USABLE {
            max_addr = max_addr.max(entry.base + entry.length);
        }
    }
    let total_frames = (max_addr as usize + FRAME_SIZE - 1) / FRAME_SIZE;

    let mut fa = FrameAllocator::new(0, total_frames);
    for i in 0..total_frames {
        fa.bitmap.set(i);
    }
    for entry in memmap.entries() {
        if entry.entry_type == EntryType::USABLE {
            fa.mark_free_range(entry.base, entry.base + entry.length);
        }
    }
    fa.mark_used_range(0, 0x100000);
    // Also mark the heap's physical range as used (it was carved out of a
    // USABLE region before the frame allocator existed).
    let (hb, he) = crate::mem::heap::heap_phys_range();
    if he > hb {
        fa.mark_used_range(hb, he);
    }

    *FRAME_ALLOC.lock() = Some(fa);
}

pub fn alloc_frame() -> Option<u64> {
    FRAME_ALLOC.lock().as_mut()?.alloc()
}

#[allow(dead_code)]
pub fn free_frame(pa: u64) {
    if let Some(fa) = FRAME_ALLOC.lock().as_mut() {
        fa.free(pa);
    }
}

pub fn stats() -> Option<(usize, usize, usize)> {
    let guard = FRAME_ALLOC.lock();
    let fa = guard.as_ref()?;
    Some((fa.total_frames(), fa.used_frames(), fa.free_frames()))
}
