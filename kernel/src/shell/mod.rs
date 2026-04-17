//! Interactive bash-like shell. Runs as a kernel thread, reads input from
//! serial + PS/2 keyboard, executes built-in commands, writes to the console.

use alloc::format;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use core::sync::atomic::{AtomicBool, Ordering};

use shared::shell::tokenize;

use crate::arch::x86_64::{keyboard, serial};
use crate::{fs, println, print};

/// Banner shown at startup.
pub const BANNER: &str = "\
  ____                  _ _                 ____  ____\n\
 / ___|  __ _ _ __   __| | |__   _____  __/ __ \\/ ___|\n\
 \\___ \\ / _` | '_ \\ / _` | '_ \\ / _ \\ \\/ / |  | \\___ \\\n\
  ___) | (_| | | | | (_| | |_) | (_) >  <| |__| |___) |\n\
 |____/ \\__,_|_| |_|\\__,_|_.__/ \\___/_/\\_\\\\____/|____/\n";

static SHUTDOWN_REQUESTED: AtomicBool = AtomicBool::new(false);
static REBOOT_REQUESTED: AtomicBool = AtomicBool::new(false);

pub fn is_shutdown_requested() -> bool {
    SHUTDOWN_REQUESTED.load(Ordering::Relaxed)
}

pub fn is_reboot_requested() -> bool {
    REBOOT_REQUESTED.load(Ordering::Relaxed)
}

/// Input events returned by the input poller.
#[derive(Clone, Copy, PartialEq)]
enum InputEvent {
    Char(char),
    ArrowUp,
    ArrowDown,
}

/// Serial ANSI-escape state machine for detecting arrow keys.
static mut SERIAL_ESC_STATE: u8 = 0; // 0=normal, 1=got ESC, 2=got ESC[

/// Read an input event (non-blocking) from serial or PS/2 keyboard.
fn poll_input_event() -> Option<InputEvent> {
    // --- serial with ANSI escape sequence parsing ---
    if let Some(b) = serial::try_read_byte() {
        unsafe {
            match SERIAL_ESC_STATE {
                0 => {
                    if b == 0x1b {
                        SERIAL_ESC_STATE = 1;
                        return None;
                    }
                    return Some(InputEvent::Char(b as char));
                }
                1 => {
                    if b == b'[' {
                        SERIAL_ESC_STATE = 2;
                        return None;
                    }
                    SERIAL_ESC_STATE = 0;
                    return Some(InputEvent::Char(b as char));
                }
                2 => {
                    SERIAL_ESC_STATE = 0;
                    return match b {
                        b'A' => Some(InputEvent::ArrowUp),
                        b'B' => Some(InputEvent::ArrowDown),
                        _ => None,
                    };
                }
                _ => { SERIAL_ESC_STATE = 0; }
            }
        }
    }
    // --- PS/2 keyboard (uses sentinel chars from the keyboard driver) ---
    if let Some(ch) = keyboard::try_read_char() {
        return match ch {
            '\x11' => Some(InputEvent::ArrowUp),
            '\x12' => Some(InputEvent::ArrowDown),
            _ => Some(InputEvent::Char(ch)),
        };
    }
    None
}

/// Erase the current line content from the screen, replace it with `new`,
/// and update `buf` to match.
fn replace_line(buf: &mut String, new: &str) {
    // Move cursor back and clear to end-of-line.
    for _ in 0..buf.len() {
        print!("\u{0008}");
    }
    // Overwrite with spaces then move back again (for terminals without EL).
    for _ in 0..buf.len() {
        print!(" ");
    }
    for _ in 0..buf.len() {
        print!("\u{0008}");
    }
    buf.clear();
    buf.push_str(new);
    print!("{}", buf);
}

/// Blocking readline with command history (up/down arrows).
fn readline<'a>(buf: &'a mut String, history: &[String]) -> &'a str {
    buf.clear();
    // Index into history: history.len() means "current (new) line".
    let mut hist_idx = history.len();
    // Stash the in-progress line when the user starts browsing history.
    let mut saved_line = String::new();
    loop {
        let ev = loop {
            if let Some(e) = poll_input_event() {
                break e;
            }
            crate::sched::yield_now();
        };
        match ev {
            InputEvent::Char('\n') | InputEvent::Char('\r') => {
                println!();
                return buf.as_str();
            }
            InputEvent::Char('\u{0008}') | InputEvent::Char('\u{007F}') => {
                if buf.pop().is_some() {
                    print!("\u{0008} \u{0008}");
                }
            }
            InputEvent::ArrowUp => {
                if hist_idx > 0 {
                    if hist_idx == history.len() {
                        saved_line = buf.clone();
                    }
                    hist_idx -= 1;
                    replace_line(buf, &history[hist_idx]);
                }
            }
            InputEvent::ArrowDown => {
                if hist_idx < history.len() {
                    hist_idx += 1;
                    if hist_idx == history.len() {
                        let s = saved_line.clone();
                        replace_line(buf, &s);
                    } else {
                        replace_line(buf, &history[hist_idx]);
                    }
                }
            }
            InputEvent::Char(c) if c.is_ascii() && (c as u32) >= 0x20 => {
                buf.push(c);
                let mut s = [0u8; 4];
                print!("{}", c.encode_utf8(&mut s));
            }
            _ => {}
        }
    }
}

pub fn run() {
    // Colorize the banner so both the framebuffer console and the VGA
    // text-mode console exercise the ANSI parser and show non-default
    // colors. The serial terminal renders the same escapes natively.
    print!("\x1b[1;36m"); // bright cyan
    println!("{}", BANNER);
    print!("\x1b[0m");
    if let Ok(motd) = fs::read("/etc/motd") {
        if let Ok(s) = core::str::from_utf8(&motd) {
            print!("\x1b[1;32m{}\x1b[0m", s); // green motd
        }
    }
    println!("Type '\x1b[1;33mhelp\x1b[0m' for a list of commands.\n");

    let mut line = String::new();
    let mut history: Vec<String> = Vec::new();
    loop {
        if SHUTDOWN_REQUESTED.load(Ordering::Relaxed)
            || REBOOT_REQUESTED.load(Ordering::Relaxed)
        {
            println!();
            return;
        }
        let cwd = fs::cwd();
        print!("{} $ ", cwd);
        let raw = readline(&mut line, &history).to_string();
        if raw.is_empty() {
            continue;
        }
        history.push(raw.clone());
        let tokens = match tokenize(&raw) {
            Ok(t) => t,
            Err(e) => {
                println!("parse error: {:?}", e);
                continue;
            }
        };
        if tokens.is_empty() {
            continue;
        }

        // Split tokens on "|" for pipe support.
        let mut segments: Vec<&[String]> = Vec::new();
        let mut start = 0;
        for (i, t) in tokens.iter().enumerate() {
            if t == "|" {
                segments.push(&tokens[start..i]);
                start = i + 1;
            }
        }
        segments.push(&tokens[start..]);

        if segments.iter().any(|s| s.is_empty()) {
            println!("parse error: empty pipe segment");
            continue;
        }

        if segments.len() == 1 {
            dispatch(&tokens, None, None);
        } else {
            let mut pipe_buf: Option<Vec<u8>> = None;
            for (i, seg) in segments.iter().enumerate() {
                let is_last = i == segments.len() - 1;
                if is_last {
                    dispatch(seg, pipe_buf.as_deref(), None);
                } else {
                    let mut out = Vec::new();
                    dispatch(seg, pipe_buf.as_deref(), Some(&mut out));
                    pipe_buf = Some(out);
                }
            }
        }
    }
}

fn _unused_exit() {}

fn dispatch(args: &[String], pipe_in: Option<&[u8]>, pipe_out: Option<&mut Vec<u8>>) {
    let cmd = args[0].as_str();
    let rest: &[String] = &args[1..];
    match cmd {
        "help" => cmd_help(),
        "echo" => cmd_echo(rest),
        "clear" | "cls" => {
            print!("\x1b[2J\x1b[H");
        }
        "ls" => cmd_ls(rest),
        "cat" => cmd_cat(rest),
        "cd" => cmd_cd(rest),
        "pwd" => println!("{}", fs::cwd()),
        "mkdir" => cmd_mkdir(rest),
        "touch" => cmd_touch(rest),
        "write" => cmd_write(rest),
        "rm" => cmd_rm(rest),
        "mem" => cmd_mem(),
        "getrambytes" => cmd_getrambytes(),
        "cpus" => cmd_cpus(),
        "ps" => cmd_ps(),
        "uptime" => cmd_uptime(),
        "spawn" => cmd_spawn(rest),
        "sleep" => cmd_sleep(rest),
        "uname" => cmd_uname(),
        "threadtest" => cmd_threadtest(rest),
        "readram" => cmd_readram(rest, pipe_out),
        "writeram" => cmd_writeram(rest, pipe_in),
        "memsetram" => cmd_memsetram(rest),
        "findtextram" => cmd_findtextram(rest),
        "replacetextram" => cmd_replacetextram(rest),
        "xxd" => cmd_xxd(pipe_in),
        "shutdown" | "poweroff" | "exit" => {
            SHUTDOWN_REQUESTED.store(true, Ordering::SeqCst);
        }
        "reboot" => {
            REBOOT_REQUESTED.store(true, Ordering::SeqCst);
        }
        _ => println!("{}: command not found", cmd),
    }
}

fn cmd_help() {
    println!("SandboxOS shell built-ins:");
    println!("  help            Show this help.");
    println!("  echo [ARGS..]   Print arguments.");
    println!("  clear           Clear the screen.");
    println!("  ls [DIR]        List directory contents.");
    println!("  cat FILE        Print file contents.");
    println!("  cd DIR          Change directory.");
    println!("  pwd             Print working directory.");
    println!("  mkdir DIR       Create a directory.");
    println!("  touch FILE      Create empty file if missing.");
    println!("  write FILE ...  Write args (joined by space) into FILE.");
    println!("  rm PATH         Remove a file or empty directory.");
    println!("  mem             Show memory statistics.");
    println!("  getrambytes     Print total RAM in bytes.");
    println!("  cpus            Show online CPUs.");
    println!("  ps              List threads.");
    println!("  uptime          Show scheduler ticks.");
    println!("  spawn N         Spawn N counter threads (for demo).");
    println!("  sleep N         Sleep N ticks.");
    println!("  threadtest N K  Run N threads, each doing K increments; verify result.");
    println!("  uname           Print OS name and version.");
    println!("  readram OFF N   Read N bytes at offset OFF (raw output).");
    println!("  writeram OFF .. Write data at offset OFF.");
    println!("  memsetram O N V Fill N bytes at offset O with byte value V.");
    println!("  findtextram TXT Search all RAM for TXT, print matching offsets.");
    println!("  replacetextram F R  Replace all occurrences of F with R in RAM (same len).");
    println!("  xxd             Hex-dump piped input (e.g. readram .. | xxd).");
    println!("  shutdown        Halt the system.");
    println!("  reboot          Reboot the system.");
}

fn cmd_echo(args: &[String]) {
    let mut first = true;
    for a in args {
        if !first {
            print!(" ");
        }
        print!("{}", a);
        first = false;
    }
    println!();
}

fn cmd_ls(args: &[String]) {
    let path = args.first().map(|s| s.as_str()).unwrap_or(".");
    match fs::list(path) {
        Ok((rows, _)) => {
            for (name, is_dir) in rows {
                if is_dir {
                    println!("{}/", name);
                } else {
                    println!("{}", name);
                }
            }
        }
        Err(e) => println!("ls: {:?}", e),
    }
}

fn cmd_cat(args: &[String]) {
    if args.is_empty() {
        println!("usage: cat FILE");
        return;
    }
    for path in args {
        match fs::read(path) {
            Ok(data) => match core::str::from_utf8(&data) {
                Ok(s) => print!("{}", s),
                Err(_) => {
                    // binary: print hex
                    for b in &data {
                        print!("{:02x} ", b);
                    }
                    println!();
                }
            },
            Err(e) => println!("cat {}: {:?}", path, e),
        }
    }
}

fn cmd_cd(args: &[String]) {
    let path = args.first().map(|s| s.as_str()).unwrap_or("/");
    if let Err(e) = fs::chdir(path) {
        println!("cd: {:?}", e);
    }
}

fn cmd_mkdir(args: &[String]) {
    if args.is_empty() {
        println!("usage: mkdir DIR");
        return;
    }
    for a in args {
        if let Err(e) = fs::mkdir(a) {
            println!("mkdir {}: {:?}", a, e);
        }
    }
}

fn cmd_touch(args: &[String]) {
    if args.is_empty() {
        println!("usage: touch FILE");
        return;
    }
    for a in args {
        if let Err(e) = fs::touch(a) {
            println!("touch {}: {:?}", a, e);
        }
    }
}

fn cmd_write(args: &[String]) {
    if args.len() < 2 {
        println!("usage: write FILE CONTENT...");
        return;
    }
    let mut content = String::new();
    for (i, a) in args[1..].iter().enumerate() {
        if i > 0 {
            content.push(' ');
        }
        content.push_str(a);
    }
    content.push('\n');
    if let Err(e) = fs::write(&args[0], content.as_bytes()) {
        println!("write: {:?}", e);
    }
}

fn cmd_rm(args: &[String]) {
    if args.is_empty() {
        println!("usage: rm PATH");
        return;
    }
    for a in args {
        if let Err(e) = fs::rm(a) {
            println!("rm {}: {:?}", a, e);
        }
    }
}

fn cmd_mem() {
    if let Some((total, used, free)) = crate::mem::pmm::stats() {
        let fs = 4096usize;
        println!(
            "frames: total={} ({} MiB), used={} ({} MiB), free={} ({} MiB)",
            total,
            total * fs / (1024 * 1024),
            used,
            used * fs / (1024 * 1024),
            free,
            free * fs / (1024 * 1024),
        );
    }
    let (h_used, h_free) = crate::mem::heap::stats();
    println!(
        "heap:   used={} KiB, free={} KiB",
        h_used / 1024,
        h_free / 1024
    );
}

fn cmd_getrambytes() {
    if let Some((total, _, _)) = crate::mem::pmm::stats() {
        println!("{}", total * 4096);
    } else {
        println!("getrambytes: memory info unavailable");
    }
}

fn cmd_cpus() {
    let rows = crate::sched::cpu_ran_counts();
    println!(
        "CPUs: {} total, {} online",
        rows.len(),
        rows.iter().filter(|r| r.2).count()
    );
    for (i, (lapic_id, ran, online, irqs)) in rows.iter().enumerate() {
        println!(
            "  cpu{}: lapic_id={} online={} scheduled_runs={} timer_irqs={}",
            i, lapic_id, online, ran, irqs
        );
    }
}

fn cmd_ps() {
    let rows = crate::sched::list_threads();
    println!("{:>4}  {:<16}  {:<10}  {}", "TID", "NAME", "STATE", "TICKS");
    for (tid, name, state, ticks) in rows {
        println!("{:>4}  {:<16}  {:<10?}  {}", tid, name, state, ticks);
    }
}

fn cmd_uptime() {
    println!("uptime: {} ticks", crate::sched::total_ticks());
}

fn cmd_spawn(args: &[String]) {
    let n: usize = args
        .first()
        .and_then(|s| s.parse().ok())
        .unwrap_or(3);
    for i in 0..n {
        let name = format!("worker-{}", i);
        crate::sched::spawn(&name, move || {
            for j in 0..5u32 {
                crate::println!("  [worker-{} tick {}]", i, j);
                crate::sched::sleep_ticks(10);
            }
        });
    }
    println!("spawned {} workers", n);
}

fn cmd_sleep(args: &[String]) {
    let n: u64 = args.first().and_then(|s| s.parse().ok()).unwrap_or(10);
    crate::sched::sleep_ticks(n);
}

fn cmd_uname() {
    println!("SandboxOS 0.1.0 x86_64 (SMP)");
}

/// `threadtest N K` — spawn N worker threads that each increment a shared
/// atomic counter K times. Wait for all workers to finish. Print a verifiable
/// result line that the functional test can assert against.
///
/// This really exercises:
///   - thread spawning + joining (via an atomic "done" count)
///   - shared-memory coherence across CPUs (all increments must be visible)
///   - scheduler preemption (the main thread yields while workers run)
fn cmd_threadtest(args: &[String]) {
    use alloc::sync::Arc;
    use core::sync::atomic::{AtomicU64, AtomicUsize, Ordering};

    let n: usize = args.first().and_then(|s| s.parse().ok()).unwrap_or(4);
    let k: u64 = args.get(1).and_then(|s| s.parse().ok()).unwrap_or(10_000);
    let expected = (n as u64) * k;

    let counter = Arc::new(AtomicU64::new(0));
    let done = Arc::new(AtomicUsize::new(0));
    // Track which CPUs observed the work (via LAPIC ID).
    let cpu_mask = Arc::new(AtomicU64::new(0));

    for i in 0..n {
        let c = counter.clone();
        let d = done.clone();
        let m = cpu_mask.clone();
        let name = format!("tt-{}", i);
        crate::sched::spawn(&name, move || {
            for _ in 0..k {
                c.fetch_add(1, Ordering::Relaxed);
                // Record the CPU we observed ourselves on.
                let id = crate::arch::x86_64::lapic::id();
                if id < 64 {
                    m.fetch_or(1u64 << id, Ordering::Relaxed);
                }
                // Occasionally yield so other workers + preemption run.
                if (c.load(Ordering::Relaxed) & 0xFF) == 0 {
                    crate::sched::yield_now();
                }
            }
            d.fetch_add(1, Ordering::Relaxed);
        });
    }

    // Wait (with a generous bound) for all workers to finish.
    let deadline_ticks = crate::sched::total_ticks() + 2000;
    while done.load(Ordering::Relaxed) < n {
        crate::sched::sleep_ticks(2);
        if crate::sched::total_ticks() > deadline_ticks {
            break;
        }
    }

    let got = counter.load(Ordering::Relaxed);
    let finished = done.load(Ordering::Relaxed);
    let mask = cpu_mask.load(Ordering::Relaxed);
    let cpus_observed = mask.count_ones();
    let ok = got == expected && finished == n;
    println!(
        "THREADTEST {} workers={} k={} expected={} got={} finished={} cpus_observed={} mask={:#x}",
        if ok { "PASS" } else { "FAIL" },
        n,
        k,
        expected,
        got,
        finished,
        cpus_observed,
        mask
    );
}

/// Parse a usize from a string. Supports decimal and `0x`/`0X` hex prefix.
fn parse_usize(s: &str) -> Option<usize> {
    if let Some(hex) = s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
        usize::from_str_radix(hex, 16).ok()
    } else {
        s.parse().ok()
    }
}

/// Disable CR0.WP (Write Protect) so supervisor can write to read-only pages.
/// Returns the original CR0 value for later restoration.
unsafe fn disable_write_protect() -> u64 {
    let cr0: u64;
    core::arch::asm!("mov {}, cr0", out(reg) cr0);
    let new_cr0 = cr0 & !(1u64 << 16); // clear WP bit
    core::arch::asm!("mov cr0, {}", in(reg) new_cr0);
    cr0
}

/// Restore CR0 to a previously saved value.
unsafe fn restore_write_protect(cr0: u64) {
    core::arch::asm!("mov cr0, {}", in(reg) cr0);
}

/// `readram <offset> <num_bytes>` — read raw bytes from physical memory.
/// Output goes to pipe buffer when piped, otherwise raw bytes to console.
fn cmd_readram(args: &[String], pipe_out: Option<&mut Vec<u8>>) {
    if args.len() < 2 {
        println!("usage: readram <offset> <num_bytes>");
        return;
    }
    let offset = match parse_usize(&args[0]) {
        Some(v) => v,
        None => { println!("readram: invalid offset"); return; }
    };
    let num_bytes = match parse_usize(&args[1]) {
        Some(v) => v,
        None => { println!("readram: invalid length"); return; }
    };

    let ptr = crate::mem::pmm::phys_to_virt(offset as u64) as *const u8;
    let slice = unsafe { core::slice::from_raw_parts(ptr, num_bytes) };

    if let Some(buf) = pipe_out {
        buf.extend_from_slice(slice);
    } else {
        for &b in slice {
            print!("{}", b as char);
        }
        println!();
    }
}

/// `writeram <offset> <data...>` — write data at a memory offset.
/// Accepts piped input or space-joined arguments as data.
fn cmd_writeram(args: &[String], pipe_in: Option<&[u8]>) {
    if args.is_empty() {
        println!("usage: writeram <offset> <data...>");
        println!("       <cmd> | writeram <offset>");
        return;
    }
    let offset = match parse_usize(&args[0]) {
        Some(v) => v,
        None => { println!("writeram: invalid offset"); return; }
    };

    let ptr = crate::mem::pmm::phys_to_virt(offset as u64);

    unsafe {
        let saved_cr0 = disable_write_protect();
        if let Some(data) = pipe_in {
            core::ptr::copy_nonoverlapping(data.as_ptr(), ptr, data.len());
        } else {
            if args.len() < 2 {
                restore_write_protect(saved_cr0);
                println!("usage: writeram <offset> <data...>");
                return;
            }
            let mut content = String::new();
            for (i, a) in args[1..].iter().enumerate() {
                if i > 0 { content.push(' '); }
                content.push_str(a);
            }
            let bytes = content.as_bytes();
            core::ptr::copy_nonoverlapping(bytes.as_ptr(), ptr, bytes.len());
        }
        restore_write_protect(saved_cr0);
    }
}

/// `memsetram <offset> <num_bytes> <value>` — fill memory like C memset.
fn cmd_memsetram(args: &[String]) {
    if args.len() < 3 {
        println!("usage: memsetram <offset> <num_bytes> <value>");
        return;
    }
    let offset = match parse_usize(&args[0]) {
        Some(v) => v,
        None => { println!("memsetram: invalid offset"); return; }
    };
    let num_bytes = match parse_usize(&args[1]) {
        Some(v) => v,
        None => { println!("memsetram: invalid length"); return; }
    };
    let value = match parse_usize(&args[2]) {
        Some(v) if v <= 0xFF => v as u8,
        _ => { println!("memsetram: invalid byte value (0-255)"); return; }
    };
    let ptr = crate::mem::pmm::phys_to_virt(offset as u64);
    unsafe {
        let saved_cr0 = disable_write_protect();
        core::ptr::write_bytes(ptr, value, num_bytes);
        restore_write_protect(saved_cr0);
    }
}

/// `findtextram <text>` — search all of RAM for occurrences of text.
fn cmd_findtextram(args: &[String]) {
    if args.is_empty() {
        println!("usage: findtextram <text>");
        return;
    }
    let needle: Vec<u8> = args.join(" ").into_bytes();
    if needle.is_empty() {
        return;
    }

    let total_bytes = match crate::mem::pmm::stats() {
        Some((total_frames, _, _)) => total_frames * 4096,
        None => {
            println!("findtextram: memory info unavailable");
            return;
        }
    };

    let base = crate::mem::pmm::phys_to_virt(0);
    let nlen = needle.len();

    let mut i = 0;
    while i + nlen <= total_bytes {
        let mut matched = true;
        for j in 0..nlen {
            let b = unsafe { core::ptr::read_volatile(base.add(i + j)) };
            if b != needle[j] {
                matched = false;
                break;
            }
        }
        if matched {
            println!("{:#x}", i);
        }
        i += 1;
    }
}

/// `replacetextram <find> <replace>` — find and replace text in all of RAM.
/// Both strings must be the same length.
fn cmd_replacetextram(args: &[String]) {
    if args.len() != 2 {
        println!("usage: replacetextram <find> <replace>");
        return;
    }
    let find_src = args[0].as_bytes();
    let repl_src = args[1].as_bytes();
    if find_src.len() != repl_src.len() {
        println!("replacetextram: find and replace must be the same length ({} vs {})", find_src.len(), repl_src.len());
        return;
    }
    let nlen = find_src.len();
    if nlen == 0 || nlen > 256 {
        println!("replacetextram: length must be 1-256");
        return;
    }

    // Copy needle and replacement into stack buffers. Use read_volatile
    // to load from the source so the compiler cannot optimise comparisons
    // back to the original heap pointers (which the scan will mutate).
    let mut find_buf = [0u8; 256];
    let mut repl_buf = [0u8; 256];
    for i in 0..nlen {
        find_buf[i] = unsafe { core::ptr::read_volatile(find_src.as_ptr().add(i)) };
        repl_buf[i] = unsafe { core::ptr::read_volatile(repl_src.as_ptr().add(i)) };
    }
    // Prevent the compiler from tracing these buffers back to their source.
    let find_buf = core::hint::black_box(find_buf);
    let repl_buf = core::hint::black_box(repl_buf);

    let total_bytes = match crate::mem::pmm::stats() {
        Some((total_frames, _, _)) => total_frames * 4096,
        None => {
            println!("replacetextram: memory info unavailable");
            return;
        }
    };

    let base = crate::mem::pmm::phys_to_virt(0);

    // Single pass: scan + replace with WP disabled.
    let mut count = 0usize;
    unsafe {
        let saved_cr0 = disable_write_protect();
        let mut i = 0;
        while i + nlen <= total_bytes {
            let mut matched = true;
            for j in 0..nlen {
                let b = core::ptr::read_volatile(base.add(i + j));
                let expected = core::ptr::read_volatile(find_buf.as_ptr().add(j));
                if b != expected {
                    matched = false;
                    break;
                }
            }
            if matched {
                for j in 0..nlen {
                    let replacement = core::ptr::read_volatile(repl_buf.as_ptr().add(j));
                    core::ptr::write_volatile(base.add(i + j), replacement);
                }
                count += 1;
                i += nlen;
            } else {
                i += 1;
            }
        }
        restore_write_protect(saved_cr0);
    }
    println!("{} replacement(s)", count);
}

/// `xxd` — hex-dump piped input in traditional xxd format.
fn cmd_xxd(pipe_in: Option<&[u8]>) {
    let data = match pipe_in {
        Some(d) => d,
        None => {
            println!("usage: <command> | xxd");
            return;
        }
    };

    for (line_idx, chunk) in data.chunks(16).enumerate() {
        let offset = line_idx * 16;
        print!("{:08x}: ", offset);
        for j in 0..16 {
            if j < chunk.len() {
                print!("{:02x}", chunk[j]);
            } else {
                print!("  ");
            }
            if j % 2 == 1 {
                print!(" ");
            }
        }
        print!(" ");
        for &b in chunk {
            if b >= 0x20 && b <= 0x7e {
                print!("{}", b as char);
            } else {
                print!(".");
            }
        }
        println!();
    }
}
