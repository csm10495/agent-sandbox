//! Fixed-size bitmap allocator for frame/page tracking.
//!
//! Tracks up to `N * 8` bits. 0 = free, 1 = used. Used by the kernel's physical
//! frame allocator. Kept pure-logic so it can be tested on the host.

use alloc::vec;
use alloc::vec::Vec;

pub struct Bitmap {
    bits: Vec<u8>,
    capacity: usize,
    first_free_hint: usize,
    used: usize,
}

impl Bitmap {
    pub fn new(capacity: usize) -> Self {
        let bytes = capacity.div_ceil(8);
        Self {
            bits: vec![0u8; bytes],
            capacity,
            first_free_hint: 0,
            used: 0,
        }
    }

    pub fn capacity(&self) -> usize {
        self.capacity
    }

    pub fn used(&self) -> usize {
        self.used
    }

    pub fn free(&self) -> usize {
        self.capacity - self.used
    }

    pub fn is_set(&self, index: usize) -> bool {
        debug_assert!(index < self.capacity);
        (self.bits[index / 8] >> (index % 8)) & 1 == 1
    }

    pub fn set(&mut self, index: usize) {
        debug_assert!(index < self.capacity);
        let mask = 1u8 << (index % 8);
        if self.bits[index / 8] & mask == 0 {
            self.bits[index / 8] |= mask;
            self.used += 1;
        }
    }

    pub fn clear(&mut self, index: usize) {
        debug_assert!(index < self.capacity);
        let mask = 1u8 << (index % 8);
        if self.bits[index / 8] & mask != 0 {
            self.bits[index / 8] &= !mask;
            self.used -= 1;
            if index < self.first_free_hint {
                self.first_free_hint = index;
            }
        }
    }

    /// Find and mark the first free bit. Returns the index.
    pub fn alloc(&mut self) -> Option<usize> {
        let start = self.first_free_hint;
        for i in start..self.capacity {
            if !self.is_set(i) {
                self.set(i);
                self.first_free_hint = i + 1;
                return Some(i);
            }
        }
        // Wrap once.
        for i in 0..start {
            if !self.is_set(i) {
                self.set(i);
                self.first_free_hint = i + 1;
                return Some(i);
            }
        }
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn set_clear() {
        let mut b = Bitmap::new(32);
        assert_eq!(b.free(), 32);
        b.set(3);
        assert!(b.is_set(3));
        assert_eq!(b.used(), 1);
        b.clear(3);
        assert!(!b.is_set(3));
        assert_eq!(b.used(), 0);
    }

    #[test]
    fn alloc_sequential_then_exhaust() {
        let mut b = Bitmap::new(4);
        assert_eq!(b.alloc(), Some(0));
        assert_eq!(b.alloc(), Some(1));
        assert_eq!(b.alloc(), Some(2));
        assert_eq!(b.alloc(), Some(3));
        assert_eq!(b.alloc(), None);
    }

    #[test]
    fn alloc_reuses_after_clear() {
        let mut b = Bitmap::new(4);
        let _ = b.alloc();
        let _ = b.alloc();
        b.clear(0);
        assert_eq!(b.alloc(), Some(0));
    }
}
