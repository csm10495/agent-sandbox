//! CPUID helpers for topology and vendor info.

use core::arch::x86_64::__cpuid;

pub fn vendor_string() -> [u8; 12] {
    let r = unsafe { __cpuid(0) };
    let mut buf = [0u8; 12];
    buf[0..4].copy_from_slice(&r.ebx.to_le_bytes());
    buf[4..8].copy_from_slice(&r.edx.to_le_bytes());
    buf[8..12].copy_from_slice(&r.ecx.to_le_bytes());
    buf
}

pub fn brand_string() -> Option<[u8; 48]> {
    let max_ext = unsafe { __cpuid(0x8000_0000) }.eax;
    if max_ext < 0x8000_0004 {
        return None;
    }
    let mut buf = [0u8; 48];
    for (i, leaf) in (0x8000_0002..=0x8000_0004).enumerate() {
        let r = unsafe { __cpuid(leaf) };
        let off = i * 16;
        buf[off..off + 4].copy_from_slice(&r.eax.to_le_bytes());
        buf[off + 4..off + 8].copy_from_slice(&r.ebx.to_le_bytes());
        buf[off + 8..off + 12].copy_from_slice(&r.ecx.to_le_bytes());
        buf[off + 12..off + 16].copy_from_slice(&r.edx.to_le_bytes());
    }
    Some(buf)
}
