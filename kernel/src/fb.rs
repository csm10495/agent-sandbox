//! Simple framebuffer text console. Renders 8x16 PSF glyphs into a Limine
//! framebuffer. Only 32-bpp RGB framebuffers are supported; other formats
//! render as cleared only (serial still works).
//!
//! Font: Lat15-VGA16 from the Debian `console-setup` package. Console fonts
//! are public domain.

use core::ptr::write_volatile;

const FONT_W: usize = 8;
const FONT_H: usize = 16;

const FONT_PSF: &[u8] = include_bytes!("font.psf");

fn glyph(c: u8) -> &'static [u8] {
    // PSF1: 4-byte header, then 256 glyphs × FONT_H bytes.
    let off = 4 + (c as usize) * FONT_H;
    if off + FONT_H <= FONT_PSF.len() {
        &FONT_PSF[off..off + FONT_H]
    } else {
        &FONT_PSF[4..4 + FONT_H] // fallback to glyph 0
    }
}

pub struct Framebuffer {
    pub addr: *mut u8,
    pub width: usize,
    pub height: usize,
    pub pitch: usize,
    pub bpp: usize,
    cursor_col: usize,
    cursor_row: usize,
    bg: u32,
    fg: u32,
}

unsafe impl Send for Framebuffer {}
unsafe impl Sync for Framebuffer {}

impl Framebuffer {
    pub fn new(addr: *mut u8, width: usize, height: usize, pitch: usize, bpp: usize) -> Self {
        let mut fb = Self {
            addr,
            width,
            height,
            pitch,
            bpp,
            cursor_col: 0,
            cursor_row: 0,
            bg: 0x00_00_20_40,
            fg: 0x00_E8_E8_E8,
        };
        fb.clear();
        fb
    }

    #[inline(always)]
    fn put_pixel(&self, x: usize, y: usize, rgba: u32) {
        if x >= self.width || y >= self.height || self.bpp != 32 {
            return;
        }
        let offset = y * self.pitch + x * 4;
        unsafe {
            write_volatile(self.addr.add(offset) as *mut u32, rgba);
        }
    }

    pub fn clear(&mut self) {
        if self.bpp != 32 {
            return;
        }
        for y in 0..self.height {
            for x in 0..self.width {
                self.put_pixel(x, y, self.bg);
            }
        }
        self.cursor_col = 0;
        self.cursor_row = 0;
    }

    fn cols(&self) -> usize {
        self.width / FONT_W
    }

    fn rows(&self) -> usize {
        self.height / FONT_H
    }

    fn scroll_one_row(&mut self) {
        if self.bpp != 32 {
            return;
        }
        let row_bytes = self.pitch * FONT_H;
        let total = self.pitch * self.height;
        unsafe {
            core::ptr::copy(self.addr.add(row_bytes), self.addr, total - row_bytes);
            let start_y = self.height - FONT_H;
            for y in start_y..self.height {
                for x in 0..self.width {
                    let off = y * self.pitch + x * 4;
                    write_volatile(self.addr.add(off) as *mut u32, self.bg);
                }
            }
        }
    }

    fn draw_glyph(&self, col: usize, row: usize, ch: u8) {
        let px = col * FONT_W;
        let py = row * FONT_H;
        let g = glyph(ch);
        for (ry, byte) in g.iter().enumerate() {
            for bx in 0..FONT_W {
                let bit = byte & (0x80 >> bx) != 0;
                let color = if bit { self.fg } else { self.bg };
                self.put_pixel(px + bx, py + ry, color);
            }
        }
    }

    pub fn write_str(&mut self, s: &str) {
        for &b in s.as_bytes() {
            match b {
                b'\n' => {
                    self.cursor_col = 0;
                    self.cursor_row += 1;
                }
                b'\r' => {
                    self.cursor_col = 0;
                }
                b'\t' => {
                    let next = (self.cursor_col + 8) & !7;
                    self.cursor_col = next.min(self.cols());
                }
                0x08 => {
                    if self.cursor_col > 0 {
                        self.cursor_col -= 1;
                        self.draw_glyph(self.cursor_col, self.cursor_row, b' ');
                    }
                }
                0x20..=0x7E => {
                    if self.cursor_col >= self.cols() {
                        self.cursor_col = 0;
                        self.cursor_row += 1;
                    }
                    self.draw_glyph(self.cursor_col, self.cursor_row, b);
                    self.cursor_col += 1;
                }
                _ => {
                    // Ignore non-printables for now.
                }
            }
            if self.cursor_row >= self.rows() {
                self.scroll_one_row();
                self.cursor_row = self.rows() - 1;
            }
        }
    }
}
