//! Interactive bash-like shell. Runs as a kernel thread, reads input from
//! serial + PS/2 keyboard, executes built-in commands, writes to the console.

use alloc::format;
use alloc::string::{String, ToString};
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

/// Read a char (non-blocking) from either serial or PS/2 keyboard.
fn poll_input_char() -> Option<char> {
    if let Some(b) = serial::try_read_byte() {
        return Some(b as char);
    }
    keyboard::try_read_char()
}

/// Blocking readline — returns completed line (without the trailing '\n').
/// Echoes characters as they are typed. Supports simple backspace editing.
fn readline(buf: &mut String) -> &str {
    buf.clear();
    loop {
        // Yield while waiting for input so other threads run.
        let ch = loop {
            if let Some(c) = poll_input_char() {
                break c;
            }
            crate::sched::yield_now();
        };
        match ch {
            '\n' | '\r' => {
                println!();
                return buf.as_str();
            }
            '\u{0008}' | '\u{007F}' => {
                // Backspace / DEL
                if buf.pop().is_some() {
                    // Erase the character on screen: write BS, space, BS.
                    print!("\u{0008} \u{0008}");
                }
            }
            c if c.is_ascii() && (c as u32) >= 0x20 => {
                buf.push(c);
                let mut s = [0u8; 4];
                print!("{}", c.encode_utf8(&mut s));
            }
            _ => {}
        }
    }
}

pub fn run() {
    println!("{}", BANNER);
    if let Ok(motd) = fs::read("/etc/motd") {
        if let Ok(s) = core::str::from_utf8(&motd) {
            print!("{}", s);
        }
    }
    println!("Type 'help' for a list of commands.\n");

    let mut line = String::new();
    loop {
        if SHUTDOWN_REQUESTED.load(Ordering::Relaxed)
            || REBOOT_REQUESTED.load(Ordering::Relaxed)
        {
            println!();
            return;
        }
        let cwd = fs::cwd();
        print!("{} $ ", cwd);
        let raw = readline(&mut line).to_string();
        if raw.is_empty() {
            continue;
        }
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
        dispatch(&tokens);
    }
}

fn _unused_exit() {}

fn dispatch(args: &[String]) {
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
        "cpus" => cmd_cpus(),
        "ps" => cmd_ps(),
        "uptime" => cmd_uptime(),
        "spawn" => cmd_spawn(rest),
        "sleep" => cmd_sleep(rest),
        "uname" => cmd_uname(),
        "threadtest" => cmd_threadtest(rest),
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
    println!("  cpus            Show online CPUs.");
    println!("  ps              List threads.");
    println!("  uptime          Show scheduler ticks.");
    println!("  spawn N         Spawn N counter threads (for demo).");
    println!("  sleep N         Sleep N ticks.");
    println!("  threadtest N K  Run N threads, each doing K increments; verify result.");
    println!("  uname           Print OS name and version.");
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
