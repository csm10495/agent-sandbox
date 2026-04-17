//! xtask — build driver + automated VM test harness for SandboxOS.
//!
//! Subcommands:
//!   build           Build kernel in debug mode.
//!   build-release   Build kernel in release mode.
//!   iso             Build kernel + produce `sandboxos.iso`.
//!   run             Build ISO + run in QEMU (serial on stdio, with display).
//!   test            Build ISO + run automated QEMU functional test (BIOS).
//!   test-bios       Same as `test`, BIOS boot only.
//!   test-uefi       Same as `test`, UEFI boot via OVMF firmware.
//!   test-all        Run `test-bios` and `test-uefi`. Also KVM variants when
//!                   /dev/kvm is usable.
//!   clean           Remove build artifacts.

use std::env;
use std::fs;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::process::{exit, Command, Stdio};
use std::time::{Duration, Instant};

fn main() {
    let args: Vec<String> = env::args().collect();
    let cmd = args.get(1).map(|s| s.as_str()).unwrap_or("help");
    let rest: Vec<String> = args.iter().skip(2).cloned().collect();
    let res = match cmd {
        "build" => build(false),
        "build-release" => build(true),
        "iso" => iso(true).map(|_| ()),
        "run" => run(&rest),
        "test" | "test-bios" => run_test(VmConfig::bios()),
        "test-uefi" => run_test(VmConfig::uefi()),
        "test-all" => test_all(),
        "clean" => clean(),
        _ => {
            eprintln!(
                "xtask subcommands: build | build-release | iso | run | test | test-bios | test-uefi | test-all | clean"
            );
            return;
        }
    };
    if let Err(e) = res {
        eprintln!("xtask: FAIL: {}", e);
        exit(1);
    }
}

fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .to_path_buf()
}

fn must(cmd: &mut Command) -> Result<(), String> {
    let status = cmd
        .status()
        .map_err(|e| format!("failed to spawn {:?}: {}", cmd, e))?;
    if !status.success() {
        return Err(format!("command failed: {:?} (status {})", cmd, status));
    }
    Ok(())
}

fn build(release: bool) -> Result<(), String> {
    let root = workspace_root();
    let mut c = Command::new("cargo");
    c.current_dir(&root)
        .args(["build", "-p", "kernel", "--target", "x86_64-unknown-none"]);
    if release {
        c.arg("--release");
    }
    must(&mut c)
}

fn kernel_binary(release: bool) -> PathBuf {
    let root = workspace_root();
    let sub = if release { "release" } else { "debug" };
    root.join("target/x86_64-unknown-none")
        .join(sub)
        .join("kernel")
}

fn iso(release: bool) -> Result<PathBuf, String> {
    build(release)?;
    let root = workspace_root();
    let staging = root.join("target/iso_root");
    let _ = fs::remove_dir_all(&staging);
    fs::create_dir_all(staging.join("boot/limine")).map_err(|e| e.to_string())?;
    fs::create_dir_all(staging.join("EFI/BOOT")).map_err(|e| e.to_string())?;

    let kernel = kernel_binary(release);
    fs::copy(&kernel, staging.join("boot/kernel.elf")).map_err(|e| e.to_string())?;
    fs::copy(
        root.join("boot/limine.cfg"),
        staging.join("boot/limine/limine.cfg"),
    )
    .map_err(|e| e.to_string())?;

    for name in ["limine-bios.sys", "limine-bios-cd.bin", "limine-uefi-cd.bin"] {
        fs::copy(
            root.join("boot/limine").join(name),
            staging.join("boot/limine").join(name),
        )
        .map_err(|e| e.to_string())?;
    }
    fs::copy(
        root.join("boot/limine/BOOTX64.EFI"),
        staging.join("EFI/BOOT/BOOTX64.EFI"),
    )
    .map_err(|e| e.to_string())?;
    fs::copy(
        root.join("boot/limine/BOOTIA32.EFI"),
        staging.join("EFI/BOOT/BOOTIA32.EFI"),
    )
    .map_err(|e| e.to_string())?;

    let iso_path = root.join("target/sandboxos.iso");
    let _ = fs::remove_file(&iso_path);
    must(
        Command::new("xorriso")
            .args([
                "-as",
                "mkisofs",
                "-R",
                "-r",
                "-J",
                "-b",
                "boot/limine/limine-bios-cd.bin",
                "-no-emul-boot",
                "-boot-load-size",
                "4",
                "-boot-info-table",
                "--efi-boot",
                "boot/limine/limine-uefi-cd.bin",
                "-efi-boot-part",
                "--efi-boot-image",
                "--protective-msdos-label",
            ])
            .arg(&staging)
            .arg("-o")
            .arg(&iso_path),
    )?;

    must(
        Command::new(root.join("boot/limine/limine-host-tool"))
            .arg("bios-install")
            .arg(&iso_path),
    )?;

    println!("ISO built: {}", iso_path.display());
    Ok(iso_path)
}

fn clean() -> Result<(), String> {
    must(Command::new("cargo").arg("clean"))?;
    Ok(())
}

#[derive(Clone)]
struct VmConfig {
    name: String,
    machine: String,
    smp: u32,
    uefi: bool,
    kvm: bool,
}

impl VmConfig {
    fn bios() -> Self {
        Self {
            name: "bios-q35-smp4-tcg".into(),
            machine: "q35".into(),
            smp: 4,
            uefi: false,
            kvm: false,
        }
    }
    fn uefi() -> Self {
        Self {
            name: "uefi-q35-smp4-tcg".into(),
            machine: "q35".into(),
            smp: 4,
            uefi: true,
            kvm: false,
        }
    }
    fn with_kvm(mut self) -> Self {
        self.kvm = true;
        self.name = format!("{}-kvm", self.name.replace("-tcg", ""));
        self
    }
    fn with_smp(mut self, n: u32) -> Self {
        self.smp = n;
        self
    }
    fn with_machine(mut self, m: &str) -> Self {
        self.machine = m.into();
        self
    }
}

fn qemu_cmd(iso: &Path, cfg: &VmConfig) -> Result<Command, String> {
    let mut c = Command::new("qemu-system-x86_64");
    c.args(["-M", &cfg.machine, "-m", "256M", "-smp", &cfg.smp.to_string()]);
    c.args(["-cdrom"]).arg(iso);
    c.args([
        "-boot",
        "d",
        "-device",
        "isa-debug-exit,iobase=0xf4,iosize=0x04",
        "-no-reboot",
        "-display",
        "none",
        "-serial",
        "mon:stdio",
    ]);
    if cfg.uefi {
        let ovmf_code = pick_first_existing(&[
            "/usr/share/OVMF/OVMF_CODE_4M.fd",
            "/usr/share/OVMF/OVMF_CODE.fd",
            "/usr/share/ovmf/OVMF.fd",
        ])
        .ok_or("UEFI firmware (OVMF) not found")?;
        // Use a writable copy of OVMF vars so NV storage works.
        let root = workspace_root();
        let vars_src = pick_first_existing(&[
            "/usr/share/OVMF/OVMF_VARS_4M.fd",
            "/usr/share/OVMF/OVMF_VARS.fd",
        ])
        .ok_or("OVMF vars not found")?;
        let vars_dst = root.join("target/OVMF_VARS.fd");
        fs::copy(&vars_src, &vars_dst).map_err(|e| e.to_string())?;
        c.args([
            "-drive",
            &format!("if=pflash,format=raw,unit=0,readonly=on,file={}", ovmf_code),
            "-drive",
            &format!(
                "if=pflash,format=raw,unit=1,file={}",
                vars_dst.display()
            ),
        ]);
    }
    if cfg.kvm {
        c.args(["-enable-kvm", "-cpu", "host"]);
    }
    Ok(c)
}

fn pick_first_existing(paths: &[&str]) -> Option<String> {
    for p in paths {
        if Path::new(p).exists() {
            return Some((*p).to_string());
        }
    }
    None
}

fn run(_args: &[String]) -> Result<(), String> {
    let iso = iso(false)?;
    // For interactive use, keep the display and use BIOS.
    let mut c = Command::new("qemu-system-x86_64");
    c.args(["-M", "q35", "-m", "256M", "-smp", "4", "-cdrom"])
        .arg(&iso)
        .args([
            "-boot",
            "d",
            "-device",
            "isa-debug-exit,iobase=0xf4,iosize=0x04",
            "-no-reboot",
            "-serial",
            "mon:stdio",
        ]);
    must(&mut c)
}

fn test_all() -> Result<(), String> {
    let mut configs: Vec<VmConfig> = Vec::new();
    configs.push(VmConfig::bios());
    configs.push(VmConfig::uefi());
    configs.push(VmConfig::bios().with_machine("pc")); // older SeaBIOS path
    configs.push(VmConfig::bios().with_smp(1)); // uniprocessor
    configs.push(VmConfig::bios().with_smp(8)); // 8 CPUs

    // Try KVM if /dev/kvm is accessible.
    if let Ok(meta) = fs::metadata("/dev/kvm") {
        let _ = meta; // presence is enough; actual use may still fail
        configs.push(VmConfig::bios().with_kvm());
        configs.push(VmConfig::uefi().with_kvm());
    }

    let mut failures = Vec::new();
    for cfg in &configs {
        println!("\n==== VM CONFIG: {} ====", cfg.name);
        match run_test(cfg.clone()) {
            Ok(()) => println!("---- {}: PASS", cfg.name),
            Err(e) => {
                eprintln!("---- {}: FAIL: {}", cfg.name, e);
                failures.push(cfg.name.clone());
            }
        }
    }
    println!();
    if failures.is_empty() {
        println!("ALL VM CONFIGURATIONS PASSED ({} configs)", configs.len());
        Ok(())
    } else {
        Err(format!(
            "{} configuration(s) failed: {:?}",
            failures.len(),
            failures
        ))
    }
}

/// Boot the ISO under QEMU, drive the shell over serial, and verify expected
/// behavior with *structured assertions* (not plain `contains`). The magic
/// token round-trip, CPU schedule/irq counters, preemption counts, and the
/// `threadtest` PASS line all have to match.
fn run_test(cfg: VmConfig) -> Result<(), String> {
    let iso = iso(false)?;

    const MAGIC: &str = "SANDBOXOS-MAGIC-TOKEN-7F3A1C9E";
    const AFTER_RM_SENTINEL: &str = "AFTER-RM-LISTING-SENTINEL-X";

    // Chose thread workload: big enough that workers cannot finish in a single
    // scheduler tick, so at least some migrate between CPUs in SMP configs.
    let thread_iters = if cfg.smp >= 2 { 200_000u64 } else { 50_000u64 };
    let cmds = vec![
        "uname".to_string(),
        "echo READY-BANNER-AAAA".to_string(),
        "pwd".to_string(),
        "ls /etc".to_string(),
        "cat /etc/version".to_string(),
        "mkdir /tmp/demo".to_string(),
        format!("write /tmp/demo/token.txt {}", MAGIC),
        "cat /tmp/demo/token.txt".to_string(),
        "ls /tmp/demo".to_string(),
        "rm /tmp/demo/token.txt".to_string(),
        format!("echo {}", AFTER_RM_SENTINEL),
        "ls /tmp/demo".to_string(),
        "mem".to_string(),
        "cpus".to_string(),
        format!("threadtest {} {}", cfg.smp.max(1), thread_iters),
        "sleep 5".to_string(),
        "ps".to_string(),
        "cpus".to_string(),
        "uptime".to_string(),
        "shutdown".to_string(),
    ];

    let mut child = qemu_cmd(&iso, &cfg)?
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::inherit())
        .spawn()
        .map_err(|e| format!("spawn qemu ({}): {}", cfg.name, e))?;

    let stdin = child.stdin.take().unwrap();
    let mut stdout = child.stdout.take().unwrap();

    let reader = std::thread::spawn(move || {
        let mut buf = [0u8; 4096];
        let mut all = String::new();
        loop {
            match stdout.read(&mut buf) {
                Ok(0) => break,
                Ok(n) => {
                    let chunk = String::from_utf8_lossy(&buf[..n]).to_string();
                    print!("{}", chunk);
                    std::io::stdout().flush().ok();
                    all.push_str(&chunk);
                }
                Err(_) => break,
            }
        }
        all
    });

    let mut writer = stdin;
    // Give BIOS/UEFI enough time to boot — UEFI via OVMF is slow.
    let boot_delay = if cfg.uefi { 4000 } else { 2000 };
    std::thread::sleep(Duration::from_millis(boot_delay));
    for cmd in &cmds {
        writeln!(writer, "{}", cmd).map_err(|e| e.to_string())?;
        writer.flush().ok();
        // Longer pause for threadtest to complete.
        let delay = if cmd.starts_with("threadtest") {
            3000
        } else if cmd.starts_with("sleep") {
            1500
        } else {
            500
        };
        std::thread::sleep(Duration::from_millis(delay));
    }
    drop(writer);

    let overall_deadline = if cfg.uefi {
        Duration::from_secs(90)
    } else {
        Duration::from_secs(60)
    };
    let deadline = Instant::now() + overall_deadline;
    let status = loop {
        match child.try_wait().map_err(|e| e.to_string())? {
            Some(s) => break s,
            None => {
                if Instant::now() > deadline {
                    let _ = child.kill();
                    return Err(format!(
                        "qemu ({}) did not exit within {:?}",
                        cfg.name, overall_deadline
                    ));
                }
                std::thread::sleep(Duration::from_millis(200));
            }
        }
    };

    let output = reader.join().unwrap_or_default();
    let exit_code = status.code().unwrap_or(-1);
    println!(
        "\n== [{}] QEMU exited with code {} ==",
        cfg.name, exit_code
    );

    // ===== Structured assertions =====
    let mut checks: Vec<String> = Vec::new();

    let expect_contains = |needle: &str, out: &mut Vec<String>| {
        if !output.contains(needle) {
            out.push(format!("missing expected output: {:?}", needle));
        }
    };

    // 1. Boot completed and shell came up.
    expect_contains("SandboxOS starting", &mut checks);
    expect_contains("BSP entering scheduler", &mut checks);
    expect_contains("Welcome to SandboxOS!", &mut checks);

    // 2. `uname` produced real output (not just echo). The echo is
    //    "uname\n" and the command output is "SandboxOS 0.1.0 x86_64 (SMP)".
    //    The FULL output string only appears in the real output, never in
    //    echo — so one occurrence is real.
    expect_contains("SandboxOS 0.1.0 x86_64 (SMP)", &mut checks);

    // 3. Filesystem round-trip. The MAGIC token appears:
    //    (a) once in the echo of `write /tmp/demo/token.txt <MAGIC>`, and
    //    (b) once in the output of `cat /tmp/demo/token.txt` (the `cat` echo
    //        itself does not contain the token).
    //    If the FS did not actually read it back, we'd see only (a).
    //    We require >= 2, AND we pinpoint the cat output by locating the
    //    echoed cat command and asserting the next non-empty line is the
    //    magic token, proving true read-back.
    let magic_count = output.matches(MAGIC).count();
    if magic_count < 2 {
        checks.push(format!(
            "filesystem read-back failed: magic token seen {} times (want >= 2)",
            magic_count
        ));
    }
    let cat_cmd = "cat /tmp/demo/token.txt";
    if let Some(cat_idx) = output.find(cat_cmd) {
        let after = &output[cat_idx + cat_cmd.len()..];
        let next_payload_line = after
            .lines()
            .skip(1)
            .find(|l| !l.trim().is_empty() && !l.trim_start().starts_with("/ $"))
            .map(|l| l.trim())
            .unwrap_or("");
        if !next_payload_line.contains(MAGIC) {
            checks.push(format!(
                "filesystem read-back: line after `cat` was {:?}, want to contain MAGIC",
                next_payload_line
            ));
        }
    } else {
        checks.push("filesystem read-back: `cat` command not found in output".into());
    }

    // 4. `rm` actually deleted: `ls /tmp/demo` AFTER `rm` must not show
    //    token.txt. We bracket the second ls with AFTER_RM_SENTINEL so we
    //    can isolate its window.
    if let Some(after_idx) = output.find(AFTER_RM_SENTINEL) {
        let window: &str = &output[after_idx..];
        // Find "/ $ shutdown" or end-of-stream to bound the window.
        let end = window.find("shutdown").unwrap_or(window.len());
        let window = &window[..end];
        if window.contains("token.txt") {
            checks.push(
                "filesystem remove failed: token.txt still listed after rm".into(),
            );
        }
    } else {
        checks.push("missing AFTER_RM_SENTINEL echo".into());
    }

    // 5. `threadtest` printed a PASS line with the expected counter total.
    let workers = cfg.smp.max(1);
    let expected_count = workers as u64 * thread_iters;
    let tt_line: Option<&str> = output
        .lines()
        .find(|l| l.starts_with("THREADTEST "));
    match tt_line {
        None => checks.push("threadtest: no THREADTEST line emitted".into()),
        Some(line) => {
            if !line.starts_with("THREADTEST PASS") {
                checks.push(format!("threadtest: not PASS: {}", line));
            }
            let got_ok = line
                .split_whitespace()
                .find_map(|tok| tok.strip_prefix("got="))
                .and_then(|v| v.parse::<u64>().ok())
                .map(|v| v == expected_count)
                .unwrap_or(false);
            if !got_ok {
                checks.push(format!(
                    "threadtest: got != expected {} ({})",
                    expected_count, line
                ));
            }
            let finished_ok = line
                .split_whitespace()
                .find_map(|tok| tok.strip_prefix("finished="))
                .and_then(|v| v.parse::<u32>().ok())
                .map(|v| v == workers)
                .unwrap_or(false);
            if !finished_ok {
                checks.push(format!(
                    "threadtest: finished != {} ({})",
                    workers, line
                ));
            }
            // For SMP >= 2 we expect work actually migrated across CPUs.
            if workers >= 2 {
                let observed = line
                    .split_whitespace()
                    .find_map(|tok| tok.strip_prefix("cpus_observed="))
                    .and_then(|v| v.parse::<u32>().ok())
                    .unwrap_or(0);
                if observed < 2 {
                    checks.push(format!(
                        "threadtest: cpus_observed={} want >=2 (workers did not migrate)",
                        observed
                    ));
                }
            }
        }
    }

    // 6. Every online CPU must have received timer IRQs (proving SMP is live,
    //    not just cosmetically present). Parse the LAST `cpus` block in the
    //    output so we see post-threadtest counters.
    let last_cpus_block = find_last_block(&output, "CPUs: ", "/ $");
    match last_cpus_block {
        None => checks.push("cpus: no 'CPUs:' block emitted".into()),
        Some(block) => {
            let mut online_cpus = 0usize;
            let mut cpus_with_irqs = 0usize;
            let mut cpus_that_scheduled = 0usize;
            for line in block.lines() {
                let line = line.trim_start();
                if !line.starts_with("cpu") || !line.contains("online=") {
                    continue;
                }
                let online = line.contains("online=true");
                if !online {
                    continue;
                }
                online_cpus += 1;
                if let Some(v) = line
                    .split_whitespace()
                    .find_map(|t| t.strip_prefix("timer_irqs="))
                    .and_then(|v| v.parse::<u64>().ok())
                {
                    if v > 0 {
                        cpus_with_irqs += 1;
                    }
                }
                if let Some(v) = line
                    .split_whitespace()
                    .find_map(|t| t.strip_prefix("scheduled_runs="))
                    .and_then(|v| v.parse::<u64>().ok())
                {
                    if v > 0 {
                        cpus_that_scheduled += 1;
                    }
                }
            }
            if online_cpus != cfg.smp as usize {
                checks.push(format!(
                    "cpus: {} online but expected {}",
                    online_cpus, cfg.smp
                ));
            }
            if cpus_with_irqs != online_cpus {
                checks.push(format!(
                    "cpus: only {}/{} online CPUs received timer_irqs",
                    cpus_with_irqs, online_cpus
                ));
            }
            if cpus_that_scheduled < online_cpus.min(2) {
                checks.push(format!(
                    "cpus: only {}/{} online CPUs actually scheduled",
                    cpus_that_scheduled, online_cpus
                ));
            }
        }
    }

    // 7. `ps` showed at least 3 threads with non-zero TICKS (preemption).
    let ps_block = find_last_block(&output, "TID ", "/ $");
    match ps_block {
        None => checks.push("ps: no thread-list block emitted".into()),
        Some(block) => {
            let mut with_ticks = 0usize;
            for line in block.lines().skip(1) {
                let parts: Vec<&str> = line.trim().split_whitespace().collect();
                if parts.len() < 4 {
                    continue;
                }
                if let Ok(tid) = parts[0].parse::<u64>() {
                    let _ = tid;
                    if let Ok(ticks) = parts[parts.len() - 1].parse::<u64>() {
                        if ticks > 0 {
                            with_ticks += 1;
                        }
                    }
                }
            }
            if with_ticks < 2 {
                checks.push(format!(
                    "ps: only {} thread(s) with non-zero ticks (preemption?)",
                    with_ticks
                ));
            }
        }
    }

    // 8. Clean exit via isa-debug-exit (we write 0x10, QEMU exits (0x10<<1)|1 = 0x21).
    if exit_code != 0x21 {
        checks.push(format!(
            "QEMU exit code {} (want 0x21 from shutdown command)",
            exit_code
        ));
    }

    if !checks.is_empty() {
        return Err(format!(
            "[{}] {} check(s) failed:\n  - {}",
            cfg.name,
            checks.len(),
            checks.join("\n  - ")
        ));
    }

    println!("[{}] ALL CHECKS PASSED", cfg.name);
    Ok(())
}

/// Find the last occurrence of `start_needle` in `output` and return the slice
/// from there until the next line starting with `end_needle` (or to EOF).
fn find_last_block<'a>(output: &'a str, start_needle: &str, end_needle: &str) -> Option<&'a str> {
    let idx = output.rfind(start_needle)?;
    let tail = &output[idx..];
    let end = tail.lines().skip(1).enumerate().find_map(|(i, line)| {
        if line.trim_start().starts_with(end_needle) {
            Some(i + 1)
        } else {
            None
        }
    });
    Some(match end {
        Some(end_line_idx) => {
            let mut count = 0;
            let mut cut = tail.len();
            for (i, _) in tail.match_indices('\n') {
                count += 1;
                if count == end_line_idx {
                    cut = i;
                    break;
                }
            }
            &tail[..cut]
        }
        None => tail,
    })
}
