//! xtask — build driver for SandboxOS.
//!
//! Subcommands:
//!   build          Build kernel in debug mode.
//!   build-release  Build kernel in release mode.
//!   iso            Build kernel + produce `sandboxos.iso`.
//!   run            Build ISO + run in QEMU (serial on stdio, with display).
//!   test           Build ISO + run automated QEMU functional test.
//!   clean          Remove build artifacts.

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
        "test" => test(&rest),
        "clean" => clean(),
        _ => {
            eprintln!(
                "xtask subcommands: build | build-release | iso | run | test | clean"
            );
            return;
        }
    };
    if let Err(e) = res {
        eprintln!("xtask: {}", e);
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

fn qemu_cmd(iso: &Path, headless: bool) -> Command {
    let mut c = Command::new("qemu-system-x86_64");
    c.args(["-M", "q35", "-m", "256M", "-smp", "4", "-cdrom"])
        .arg(iso)
        .args([
            "-boot",
            "d",
            "-device",
            "isa-debug-exit,iobase=0xf4,iosize=0x04",
            "-no-reboot",
        ]);
    if headless {
        c.args(["-display", "none", "-serial", "mon:stdio"]);
    } else {
        c.args(["-serial", "mon:stdio"]);
    }
    c
}

fn run(_args: &[String]) -> Result<(), String> {
    let iso = iso(false)?;
    let mut c = qemu_cmd(&iso, false);
    must(&mut c)
}

fn test(_args: &[String]) -> Result<(), String> {
    let iso = iso(false)?;
    println!("== SandboxOS functional test ==");

    let commands: &[&str] = &[
        "uname",
        "echo hello SandboxOS",
        "pwd",
        "ls /etc",
        "cat /etc/version",
        "mkdir /tmp/demo",
        "write /tmp/demo/hello.txt greetings from the shell",
        "cat /tmp/demo/hello.txt",
        "mem",
        "cpus",
        "spawn 2",
        "sleep 40",
        "ps",
        "uptime",
        "shutdown",
    ];

    let mut child = qemu_cmd(&iso, true)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::inherit())
        .spawn()
        .map_err(|e| format!("spawn qemu: {}", e))?;

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
    std::thread::sleep(Duration::from_millis(1500));
    for cmd in commands {
        writeln!(writer, "{}", cmd).map_err(|e| e.to_string())?;
        writer.flush().ok();
        std::thread::sleep(Duration::from_millis(600));
    }
    drop(writer);

    let deadline = Instant::now() + Duration::from_secs(60);
    let status = loop {
        match child.try_wait().map_err(|e| e.to_string())? {
            Some(s) => break s,
            None => {
                if Instant::now() > deadline {
                    let _ = child.kill();
                    return Err("qemu did not exit within 60s".into());
                }
                std::thread::sleep(Duration::from_millis(200));
            }
        }
    };

    let output = reader.join().unwrap_or_default();

    let expected: &[&str] = &[
        "SandboxOS starting",
        "SMP:",
        "AP online",
        "entering scheduler",
        "SandboxOS 0.1.0 x86_64 (SMP)",
        "hello SandboxOS",
        "SandboxOS 0.1.0",
        "greetings from the shell",
        "frames:",
        "heap:",
        "CPUs:",
        "online=true",
        "worker-0",
        "worker-1",
        "uptime:",
    ];
    let mut failures = Vec::new();
    for needle in expected {
        if !output.contains(needle) {
            failures.push(needle.to_string());
        }
    }

    let exit_code = status.code().unwrap_or(-1);
    println!("\n== QEMU exited with code {} ==", exit_code);

    if !failures.is_empty() {
        return Err(format!(
            "functional test missing expected output: {:?}",
            failures
        ));
    }
    // QEMU isa-debug-exit: we write 0x10, QEMU exits with (0x10<<1)|1 = 0x21.
    if exit_code != 0x21 && exit_code != 0 {
        return Err(format!("unexpected QEMU exit code {}", exit_code));
    }

    println!("FUNCTIONAL TEST OK ({} assertions)", expected.len());
    Ok(())
}
