//! Simple framebuffer text console. Renders 8x16 PSF glyphs into a Limine
//! framebuffer. Only 32-bpp RGB framebuffers are supported; other formats
//! render as cleared only (serial still works).
//!
//! This is a [`crate::term::TerminalSink`] implementation — all decoding of
//! newlines, tabs, and ANSI escape sequences is done by the shared parser.
//!
//! Font: Lat15-VGA16 from the Debian `console-setup` package. Console fonts
//! are public domain.

use core::ptr::write_volatile;

use crate::term::{Color, TerminalSink};

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

/// Default background: a dark navy that matches the banner aesthetic.
const DEFAULT_BG: u32 = 0x00_00_20_40;
/// Default foreground: near-white.
const DEFAULT_FG: u32 = 0x00_E8_E8_E8;

/// Map an ANSI color to an xRGB pixel. `bright=true` selects the bright
/// palette, matching typical xterm colors.
fn ansi_to_rgb(c: Color, bright: bool) -> u32 {
    // Colors chosen to read well on the default dark background.
    let (r, g, b) = match (c, bright) {
        (Color::Black, false) => (0x00, 0x00, 0x00),
        (Color::Black, true) => (0x55, 0x55, 0x55),
        (Color::Red, false) => (0xAA, 0x00, 0x00),
        (Color::Red, true) => (0xFF, 0x55, 0x55),
        (Color::Green, false) => (0x00, 0xAA, 0x00),
        (Color::Green, true) => (0x55, 0xFF, 0x55),
        (Color::Yellow, false) => (0xAA, 0x55, 0x00),
        (Color::Yellow, true) => (0xFF, 0xFF, 0x55),
        (Color::Blue, false) => (0x00, 0x00, 0xAA),
        (Color::Blue, true) => (0x55, 0x55, 0xFF),
        (Color::Magenta, false) => (0xAA, 0x00, 0xAA),
        (Color::Magenta, true) => (0xFF, 0x55, 0xFF),
        (Color::Cyan, false) => (0x00, 0xAA, 0xAA),
        (Color::Cyan, true) => (0x55, 0xFF, 0xFF),
        (Color::White, false) => (0xAA, 0xAA, 0xAA),
        (Color::White, true) => (0xFF, 0xFF, 0xFF),
    };
    ((r as u32) << 16) | ((g as u32) << 8) | (b as u32)
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
    bright_fg: bool,
    default_fg: u32,
    default_bg: u32,
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
            bg: DEFAULT_BG,
            fg: DEFAULT_FG,
            bright_fg: false,
            default_fg: DEFAULT_FG,
            default_bg: DEFAULT_BG,
        };
        fb.clear_screen();
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

    fn clear_screen(&mut self) {
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

    fn maybe_scroll(&mut self) {
        if self.cursor_row >= self.rows() {
            self.scroll_one_row();
            self.cursor_row = self.rows() - 1;
        }
    }
}

impl TerminalSink for Framebuffer {
    fn put_char(&mut self, ch: u8) {
        if self.cursor_col >= self.cols() {
            self.cursor_col = 0;
            self.cursor_row += 1;
            self.maybe_scroll();
        }
        self.draw_glyph(self.cursor_col, self.cursor_row, ch);
        self.cursor_col += 1;
    }

    fn newline(&mut self) {
        self.cursor_col = 0;
        self.cursor_row += 1;
        self.maybe_scroll();
    }

    fn carriage_return(&mut self) {
        self.cursor_col = 0;
    }

    fn backspace(&mut self) {
        if self.cursor_col > 0 {
            self.cursor_col -= 1;
            self.draw_glyph(self.cursor_col, self.cursor_row, b' ');
        }
    }

    fn tab(&mut self) {
        let next = (self.cursor_col + 8) & !7;
        while self.cursor_col < next && self.cursor_col < self.cols() {
            self.draw_glyph(self.cursor_col, self.cursor_row, b' ');
            self.cursor_col += 1;
        }
    }

    fn clear_all(&mut self) {
        self.clear_screen();
    }

    fn move_to(&mut self, row: usize, col: usize) {
        let rows = self.rows();
        let cols = self.cols();
        self.cursor_row = row.min(rows.saturating_sub(1));
        self.cursor_col = col.min(cols.saturating_sub(1));
    }

    fn move_rel(&mut self, drow: isize, dcol: isize) {
        let nr = (self.cursor_row as isize + drow).max(0) as usize;
        let nc = (self.cursor_col as isize + dcol).max(0) as usize;
        self.move_to(nr, nc);
    }

    fn cursor(&self) -> (usize, usize) {
        (self.cursor_row, self.cursor_col)
    }

    fn apply_sgr(&mut self, params: &[u16]) {
        for &p in params {
            match p {
                0 => {
                    self.fg = self.default_fg;
                    self.bg = self.default_bg;
                    self.bright_fg = false;
                }
                1 => {
                    self.bright_fg = true;
                    // Re-apply current fg with bright.
                }
                22 => self.bright_fg = false,
                30..=37 => {
                    if let Some(c) = Color::from_sgr(p - 30) {
                        self.fg = ansi_to_rgb(c, self.bright_fg);
                    }
                }
                39 => self.fg = self.default_fg,
                40..=47 => {
                    if let Some(c) = Color::from_sgr(p - 40) {
                        self.bg = ansi_to_rgb(c, false);
                    }
                }
                49 => self.bg = self.default_bg,
                90..=97 => {
                    if let Some(c) = Color::from_sgr(p - 90) {
                        self.fg = ansi_to_rgb(c, true);
                    }
                }
                100..=107 => {
                    if let Some(c) = Color::from_sgr(p - 100) {
                        self.bg = ansi_to_rgb(c, true);
                    }
                }
                _ => { /* ignore */ }
            }
        }
    }
}
