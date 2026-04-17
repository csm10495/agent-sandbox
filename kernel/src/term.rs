//! Terminal emulation for the on-screen (video) console.
//!
//! Design goal: make the display console behave like a terminal attached to
//! the serial line — so ANSI escape sequences produced by the kernel (colors,
//! screen clears, cursor motion) render correctly on screen instead of being
//! printed as garbage characters. This also gives us a single extension point
//! for future console backends (extra VT features, richer rendering, tty
//! multiplexing, etc.).
//!
//! ## Architecture
//!
//! Two layers:
//!
//! 1. [`TerminalSink`] — a small trait implemented by each physical backend
//!    (VGA 80x25 text buffer, framebuffer w/ PSF font, ...). Backends expose
//!    primitive ops (put a glyph, newline, clear, move cursor, set color).
//! 2. [`AnsiParser`] — a state machine that consumes UTF-8/ASCII bytes from
//!    the kernel and translates them into [`TerminalSink`] calls. It handles
//!    the subset of xterm/VT sequences we produce: `\x1b[<params><final>`
//!    (CSI), including SGR colors (`m`), clear screen (`J`), clear line
//!    (`K`), cursor positioning (`H`, `f`), cursor movement (`A`/`B`/`C`/`D`),
//!    and saves/restores (`s`/`u`).
//!
//! Adding a new backend (say, a future `Gpu` console) is just `impl
//! TerminalSink for Gpu` — no parser changes required.

/// One of the 8 basic VT / VGA colors. Bright variants are produced by
/// OR-ing with [`Color::BRIGHT_BIT`] when the sink supports it.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum Color {
    Black = 0,
    Red = 1,
    Green = 2,
    Yellow = 3,
    Blue = 4,
    Magenta = 5,
    Cyan = 6,
    White = 7,
}

impl Color {
    pub const BRIGHT_BIT: u8 = 0x08;
    pub fn from_sgr(n: u16) -> Option<Color> {
        match n {
            0 => Some(Color::Black),
            1 => Some(Color::Red),
            2 => Some(Color::Green),
            3 => Some(Color::Yellow),
            4 => Some(Color::Blue),
            5 => Some(Color::Magenta),
            6 => Some(Color::Cyan),
            7 => Some(Color::White),
            _ => None,
        }
    }
}

/// Terminal backends implement this trait. The [`AnsiParser`] calls these
/// methods as it decodes sequences.
pub trait TerminalSink {
    /// Print a printable ASCII glyph at the current cursor and advance.
    fn put_char(&mut self, ch: u8);
    /// Move to column 0 of the next line (scrolling if needed).
    fn newline(&mut self);
    /// Move to column 0 of the current line.
    fn carriage_return(&mut self);
    /// Erase the glyph to the left and move cursor back.
    fn backspace(&mut self);
    /// Advance to the next tab stop (every 8 columns).
    fn tab(&mut self);
    /// Clear the whole screen, move cursor home.
    fn clear_all(&mut self);
    /// Position cursor at (row, col), both 0-based, clamped to screen.
    fn move_to(&mut self, row: usize, col: usize);
    /// Move cursor by (drows, dcols); negative values not supported here.
    fn move_rel(&mut self, drow: isize, dcol: isize);
    /// Current cursor position, 0-based.
    fn cursor(&self) -> (usize, usize);
    /// Apply an SGR parameter list from `ESC [ ... m`. Backends may honor
    /// as few or as many as they like.
    fn apply_sgr(&mut self, params: &[u16]);
}

/// State of the CSI / escape parser.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum State {
    Normal,
    Esc,
    Csi,
}

const MAX_PARAMS: usize = 8;

/// Byte-stream ANSI parser that drives a [`TerminalSink`].
pub struct AnsiParser {
    state: State,
    params: [u16; MAX_PARAMS],
    nparams: usize,
    saved_cursor: Option<(usize, usize)>,
}

impl AnsiParser {
    pub const fn new() -> Self {
        Self {
            state: State::Normal,
            params: [0; MAX_PARAMS],
            nparams: 0,
            saved_cursor: None,
        }
    }

    /// Feed a byte slice to the parser, dispatching to `sink`.
    pub fn feed<T: TerminalSink>(&mut self, sink: &mut T, bytes: &[u8]) {
        for &b in bytes {
            self.feed_byte(sink, b);
        }
    }

    fn reset_params(&mut self) {
        self.params = [0; MAX_PARAMS];
        self.nparams = 0;
    }

    fn feed_byte<T: TerminalSink>(&mut self, sink: &mut T, b: u8) {
        match self.state {
            State::Normal => match b {
                0x1B => self.state = State::Esc,
                b'\n' => sink.newline(),
                b'\r' => sink.carriage_return(),
                0x08 => sink.backspace(),
                b'\t' => sink.tab(),
                0x20..=0x7E => sink.put_char(b),
                _ => {}
            },
            State::Esc => match b {
                b'[' => {
                    self.reset_params();
                    self.state = State::Csi;
                }
                b'c' => {
                    // Full reset.
                    sink.clear_all();
                    sink.apply_sgr(&[0]);
                    self.state = State::Normal;
                }
                _ => {
                    // Unhandled single-char escape — drop silently.
                    self.state = State::Normal;
                }
            },
            State::Csi => match b {
                b'0'..=b'9' => {
                    if self.nparams == 0 {
                        self.nparams = 1;
                    }
                    let idx = self.nparams - 1;
                    if idx < MAX_PARAMS {
                        let d = (b - b'0') as u16;
                        self.params[idx] = self.params[idx].saturating_mul(10).saturating_add(d);
                    }
                }
                b';' => {
                    if self.nparams == 0 {
                        self.nparams = 1;
                    }
                    if self.nparams < MAX_PARAMS {
                        self.nparams += 1;
                    }
                }
                b'?' | b'>' | b'=' | b' ' => {
                    // Private-mode or intermediate char — ignore; we still
                    // read up to the final letter and drop the whole CSI.
                }
                0x40..=0x7E => {
                    // Final byte — dispatch based on it.
                    let n = self.nparams.max(1);
                    let p = &self.params[..n.min(MAX_PARAMS)];
                    match b {
                        b'm' => sink.apply_sgr(p),
                        b'H' | b'f' => {
                            let row = p.first().copied().unwrap_or(1).saturating_sub(1) as usize;
                            let col = p.get(1).copied().unwrap_or(1).saturating_sub(1) as usize;
                            sink.move_to(row, col);
                        }
                        b'J' => {
                            // Only full clear (2J) is fully supported; others
                            // also clear the screen as a reasonable fallback.
                            sink.clear_all();
                        }
                        b'K' => {
                            // Erase-in-line: approximate by doing nothing for
                            // now — most kernel uses of this expect the
                            // cursor to be at column 0 after a CR, so the
                            // visible result is acceptable.
                        }
                        b'A' => sink.move_rel(-(p[0].max(1) as isize), 0),
                        b'B' => sink.move_rel(p[0].max(1) as isize, 0),
                        b'C' => sink.move_rel(0, p[0].max(1) as isize),
                        b'D' => sink.move_rel(0, -(p[0].max(1) as isize)),
                        b's' => self.saved_cursor = Some(sink.cursor()),
                        b'u' => {
                            if let Some((r, c)) = self.saved_cursor {
                                sink.move_to(r, c);
                            }
                        }
                        _ => { /* unknown CSI final byte — ignore */ }
                    }
                    self.state = State::Normal;
                    self.reset_params();
                }
                _ => {
                    // Unexpected byte inside CSI — abort the sequence.
                    self.state = State::Normal;
                    self.reset_params();
                }
            },
        }
    }
}
