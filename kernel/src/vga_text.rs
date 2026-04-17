//! Legacy VGA 80x25 color text-mode console, implemented as a
//! [`TerminalSink`] so it is driven by the shared ANSI parser.
//!
//! When Limine boots in text mode (no framebuffer request) the VGA card is
//! typically left in whatever mode the BIOS and Limine used for the boot
//! menu — often a VBE graphics mode. To guarantee the 80x25 color text
//! buffer at `0xB8000` is actually visible, [`init_hardware`] programs the
//! VGA hardware registers for BIOS mode 3 and re-uploads the 8x16 character
//! ROM to plane 2. After that, writes to `0xB8000` scan out as text glyphs.

use core::ptr::{read_volatile, write_volatile};

use crate::arch::x86_64::port;
use crate::term::{Color, TerminalSink};

const VGA_PHYS: u64 = 0xB_8000;
const VGA_PLANE2_PHYS: u64 = 0xA_0000;
pub const WIDTH: usize = 80;
pub const HEIGHT: usize = 25;

// ---- Hardware register programming ----------------------------------------

/// Standard VGA "mode 3" (80x25 16-color text) register values. Derived from
/// IBM's original VGA programming documentation; widely cross-checked.
const MISC_OUTPUT: u8 = 0x67;
const SEQ_REGS: [u8; 5] = [0x03, 0x00, 0x03, 0x00, 0x02];
const CRTC_REGS: [u8; 25] = [
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
    0x9C, 0x8E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF,
];
const GC_REGS: [u8; 9] = [0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF];
const AC_REGS: [u8; 21] = [
    // Palette entries: map AC 0..15 directly to DAC 0..15 (which we set
    // below). This avoids the traditional BIOS mode-3 mapping that pointed
    // to DAC slots 0x38..0x3F, which we would otherwise also have to
    // program.
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    // Mode control: text mode, line graphics enable, blink enable.
    0x0C, 0x00, // overscan color
    0x0F, // plane enable (all 4)
    0x08, // horizontal pixel pan
    0x00, // color select
];

/// Bytes of our 8x16 PSF1 font (glyphs only — header is 4 bytes).
const FONT_PSF: &[u8] = include_bytes!("font.psf");
const FONT_GLYPH_BYTES: usize = 16;
const FONT_NUM_GLYPHS: usize = 256;
const FONT_PLANE_SLOT: usize = 32; // VGA gives each char 32 bytes in plane 2.

/// Program the VGA hardware into BIOS mode 3 and re-upload the font to
/// plane 2. Must be called exactly once before any text rendering.
///
/// SAFETY: Writes many I/O ports and `hhdm_offset + 0xA0000..0xBFFFF`. Must
/// run only on the BSP during early boot.
pub unsafe fn init_hardware(hhdm_offset: u64) {
    // --- Disable Bochs VBE / DISPI (QEMU, VirtualBox, Bochs) ---------------
    // When Limine loaded us, the emulated card may be in a VBE graphics
    // mode; plain VGA register writes won't switch back unless we first
    // disable the VBE extension. Writing 0 to DISPI_ENABLE (index 4) does
    // that. These ports are MMIO-backed on the std-vga PCI device; the
    // legacy I/O-port pair 0x01CE/0x01CF is always the fallback.
    port::outw(0x01CE, 4); // index = ENABLE
    port::outw(0x01CF, 0); // value = 0 → VBE disabled, drop back to VGA

    // --- Miscellaneous Output ---
    port::outb(0x3C2, MISC_OUTPUT);

    // --- Sequencer: full reset, then program ---
    port::outb(0x3C4, 0x00);
    port::outb(0x3C5, 0x01); // synchronous reset
    for (i, &v) in SEQ_REGS.iter().enumerate() {
        port::outb(0x3C4, i as u8);
        port::outb(0x3C5, v);
    }

    // --- CRTC: unlock, then program ---
    // Unlock CRT0..7 by clearing bit 7 of CRT11.
    port::outb(0x3D4, 0x11);
    let cur = port::inb(0x3D5);
    port::outb(0x3D5, cur & 0x7F);
    for (i, &v) in CRTC_REGS.iter().enumerate() {
        port::outb(0x3D4, i as u8);
        port::outb(0x3D5, v);
    }

    // --- Graphics Controller ---
    for (i, &v) in GC_REGS.iter().enumerate() {
        port::outb(0x3CE, i as u8);
        port::outb(0x3CF, v);
    }

    // --- Attribute Controller ---
    // Reset flip-flop, then program. Bit 5 of the index byte selects
    // "enable display" vs "programming" — leave it clear while programming.
    let _ = port::inb(0x3DA);
    for (i, &v) in AC_REGS.iter().enumerate() {
        port::outb(0x3C0, i as u8);
        port::outb(0x3C0, v);
    }
    // Re-enable display: set bit 5 of the next index byte.
    let _ = port::inb(0x3DA);
    port::outb(0x3C0, 0x20);

    // --- Load font into plane 2 ---
    // Switch sequencer/graphics controller to "write plane 2" mode.
    port::outb(0x3C4, 0x02);
    port::outb(0x3C5, 0x04); // map mask = plane 2 only
    port::outb(0x3C4, 0x04);
    port::outb(0x3C5, 0x06); // memory mode: extended, sequential, chain4=0
    port::outb(0x3CE, 0x04);
    port::outb(0x3CF, 0x02); // read map select = plane 2
    port::outb(0x3CE, 0x05);
    port::outb(0x3CF, 0x00); // graphics mode: write mode 0, read mode 0
    port::outb(0x3CE, 0x06);
    port::outb(0x3CF, 0x04); // map to 0xA0000-0xBFFFF, graphics

    let font_base = (hhdm_offset + VGA_PLANE2_PHYS) as *mut u8;
    // PSF1 has a 4-byte header, then glyphs.
    let psf_glyphs = &FONT_PSF[4..];
    for i in 0..FONT_NUM_GLYPHS {
        let src_off = i * FONT_GLYPH_BYTES;
        let dst_off = i * FONT_PLANE_SLOT;
        for b in 0..FONT_GLYPH_BYTES {
            let byte = if src_off + b < psf_glyphs.len() {
                psf_glyphs[src_off + b]
            } else {
                0
            };
            write_volatile(font_base.add(dst_off + b), byte);
        }
        for b in FONT_GLYPH_BYTES..FONT_PLANE_SLOT {
            write_volatile(font_base.add(dst_off + b), 0);
        }
    }

    // Switch back to text mode access: planes 0+1, map B8000..BFFFF.
    port::outb(0x3C4, 0x02);
    port::outb(0x3C5, 0x03); // map mask = planes 0+1
    port::outb(0x3C4, 0x04);
    port::outb(0x3C5, 0x02); // memory mode: ext, odd/even, chain4=0
    port::outb(0x3CE, 0x04);
    port::outb(0x3CF, 0x00);
    port::outb(0x3CE, 0x05);
    port::outb(0x3CF, 0x10); // graphics mode: odd/even
    port::outb(0x3CE, 0x06);
    port::outb(0x3CF, 0x0E); // map to 0xB8000-0xBFFFF, text

    // Program the standard color palette via the DAC (matches BIOS mode 3).
    static PALETTE: [(u8, u8, u8); 16] = [
        (0, 0, 0),
        (0, 0, 42),
        (0, 42, 0),
        (0, 42, 42),
        (42, 0, 0),
        (42, 0, 42),
        (42, 21, 0),
        (42, 42, 42),
        (21, 21, 21),
        (21, 21, 63),
        (21, 63, 21),
        (21, 63, 63),
        (63, 21, 21),
        (63, 21, 63),
        (63, 63, 21),
        (63, 63, 63),
    ];
    port::outb(0x3C8, 0);
    for &(r, g, b) in &PALETTE {
        port::outb(0x3C9, r);
        port::outb(0x3C9, g);
        port::outb(0x3C9, b);
    }
}

// ---- Soft console state ---------------------------------------------------

/// Compose a VGA attribute byte from 4-bit fg + 4-bit bg indices.
#[inline(always)]
const fn attr_byte(fg: u8, bg: u8) -> u8 {
    ((bg & 0x0F) << 4) | (fg & 0x0F)
}

/// 3-bit ANSI color → 4-bit VGA palette index (low intensity).
#[inline(always)]
fn ansi_to_vga(c: Color) -> u8 {
    match c {
        Color::Black => 0,
        Color::Red => 4,
        Color::Green => 2,
        Color::Yellow => 6,
        Color::Blue => 1,
        Color::Magenta => 5,
        Color::Cyan => 3,
        Color::White => 7,
    }
}

pub struct VgaText {
    buf: *mut u16,
    row: usize,
    col: usize,
    fg: u8, // 4-bit VGA index (0..15)
    bg: u8, // 4-bit VGA index (0..15)
}

// Pointer is into HHDM; concurrent access is serialized by the console
// lock that owns this struct.
unsafe impl Send for VgaText {}

impl VgaText {
    /// SAFETY: `hhdm_offset` must be Limine's HHDM offset.
    pub unsafe fn new(hhdm_offset: u64) -> Self {
        let buf = (hhdm_offset + VGA_PHYS) as *mut u16;
        let mut this = Self {
            buf,
            row: 0,
            col: 0,
            fg: 7,
            bg: 0,
        };
        this.clear_all();
        this
    }

    #[inline(always)]
    fn attr(&self) -> u8 {
        attr_byte(self.fg, self.bg)
    }

    #[inline(always)]
    fn cell_blank(&self) -> u16 {
        ((self.attr() as u16) << 8) | b' ' as u16
    }

    fn scroll(&mut self) {
        for y in 1..HEIGHT {
            for x in 0..WIDTH {
                let src = y * WIDTH + x;
                let dst = (y - 1) * WIDTH + x;
                unsafe {
                    let v = read_volatile(self.buf.add(src));
                    write_volatile(self.buf.add(dst), v);
                }
            }
        }
        let blank = self.cell_blank();
        for x in 0..WIDTH {
            unsafe { write_volatile(self.buf.add((HEIGHT - 1) * WIDTH + x), blank) };
        }
    }

    fn update_hw_cursor(&self) {
        let pos = (self.row * WIDTH + self.col) as u16;
        unsafe {
            port::outb(0x3D4, 0x0F);
            port::outb(0x3D5, (pos & 0xFF) as u8);
            port::outb(0x3D4, 0x0E);
            port::outb(0x3D5, (pos >> 8) as u8);
        }
    }
}

impl TerminalSink for VgaText {
    fn put_char(&mut self, ch: u8) {
        if self.col >= WIDTH {
            self.newline();
        }
        let cell = ((self.attr() as u16) << 8) | ch as u16;
        unsafe {
            write_volatile(self.buf.add(self.row * WIDTH + self.col), cell);
        }
        self.col += 1;
        self.update_hw_cursor();
    }

    fn newline(&mut self) {
        self.col = 0;
        if self.row + 1 >= HEIGHT {
            self.scroll();
        } else {
            self.row += 1;
        }
        self.update_hw_cursor();
    }

    fn carriage_return(&mut self) {
        self.col = 0;
        self.update_hw_cursor();
    }

    fn backspace(&mut self) {
        if self.col > 0 {
            self.col -= 1;
            unsafe {
                write_volatile(
                    self.buf.add(self.row * WIDTH + self.col),
                    self.cell_blank(),
                );
            }
            self.update_hw_cursor();
        }
    }

    fn tab(&mut self) {
        let next = (self.col + 8) & !7;
        while self.col < next && self.col < WIDTH {
            self.put_char(b' ');
        }
    }

    fn clear_all(&mut self) {
        let blank = self.cell_blank();
        for i in 0..(WIDTH * HEIGHT) {
            unsafe { write_volatile(self.buf.add(i), blank) };
        }
        self.row = 0;
        self.col = 0;
        self.update_hw_cursor();
    }

    fn move_to(&mut self, row: usize, col: usize) {
        self.row = row.min(HEIGHT - 1);
        self.col = col.min(WIDTH - 1);
        self.update_hw_cursor();
    }

    fn move_rel(&mut self, drow: isize, dcol: isize) {
        let nr = (self.row as isize + drow).max(0) as usize;
        let nc = (self.col as isize + dcol).max(0) as usize;
        self.move_to(nr, nc);
    }

    fn cursor(&self) -> (usize, usize) {
        (self.row, self.col)
    }

    fn apply_sgr(&mut self, params: &[u16]) {
        // Handle a minimal but useful set of SGR parameters: reset, bold
        // (treated as bright fg), fg (30..37, 90..97), bg (40..47, 100..107).
        let mut bright_fg = false;
        for &p in params {
            match p {
                0 => {
                    self.fg = 7;
                    self.bg = 0;
                    bright_fg = false;
                }
                1 => bright_fg = true,
                22 => bright_fg = false,
                30..=37 => {
                    self.fg = ansi_to_vga(Color::from_sgr(p - 30).unwrap_or(Color::White));
                    if bright_fg {
                        self.fg |= 0x08;
                    }
                }
                39 => self.fg = 7,
                40..=47 => {
                    self.bg = ansi_to_vga(Color::from_sgr(p - 40).unwrap_or(Color::Black));
                }
                49 => self.bg = 0,
                90..=97 => {
                    self.fg = ansi_to_vga(Color::from_sgr(p - 90).unwrap_or(Color::White)) | 0x08;
                }
                100..=107 => {
                    self.bg = ansi_to_vga(Color::from_sgr(p - 100).unwrap_or(Color::Black)) | 0x08;
                }
                _ => { /* ignore unsupported */ }
            }
        }
    }
}
