//! Thin VFS wrapper around `shared::ramfs::RamFs` with a kernel-global mutex.

use alloc::string::String;
use alloc::vec::Vec;
use shared::ramfs::{FsError, FsResult, NodeId, RamFs};
use spin::Mutex;

static FS: Mutex<Option<RamFs>> = Mutex::new(None);

pub fn init() {
    let mut fs = RamFs::new();
    // Preload a tiny demo tree.
    fs.mkdir("/etc").ok();
    fs.mkdir("/home").ok();
    fs.mkdir("/home/user").ok();
    fs.mkdir("/tmp").ok();
    fs.create_file(
        "/etc/motd",
        b"Welcome to SandboxOS!\nType 'help' to list commands.\n",
    )
    .ok();
    fs.create_file(
        "/etc/version",
        concat!("SandboxOS 0.1.0\n", "Built with Rust edition 2021\n").as_bytes(),
    )
    .ok();
    fs.create_file(
        "/home/user/README",
        b"This is an in-memory filesystem.\nChanges are lost on reboot.\n",
    )
    .ok();
    *FS.lock() = Some(fs);
}

/// Run a closure with exclusive access to the FS. Panics if FS is not yet
/// initialized.
pub fn with<R>(f: impl FnOnce(&mut RamFs) -> R) -> R {
    let mut guard = FS.lock();
    let fs = guard.as_mut().expect("fs not initialized");
    f(fs)
}

pub fn resolve(path: &str) -> FsResult<NodeId> {
    with(|fs| fs.resolve(path))
}

pub fn read(path: &str) -> FsResult<Vec<u8>> {
    with(|fs| {
        let id = fs.resolve(path)?;
        Ok(fs.read(id)?.to_vec())
    })
}

pub fn write(path: &str, data: &[u8]) -> FsResult<()> {
    with(|fs| fs.write_file(path, data).map(|_| ()))
}

pub fn touch(path: &str) -> FsResult<()> {
    with(|fs| fs.touch(path).map(|_| ()))
}

pub fn mkdir(path: &str) -> FsResult<()> {
    with(|fs| fs.mkdir(path).map(|_| ()))
}

pub fn rm(path: &str) -> FsResult<()> {
    with(|fs| fs.remove(path))
}

pub fn chdir(path: &str) -> FsResult<()> {
    with(|fs| fs.chdir(path))
}

pub fn cwd() -> String {
    with(|fs| fs.cwd_path())
}

pub fn list(path: &str) -> FsResult<(Vec<(String, bool)>, String)> {
    with(|fs| {
        let id = fs.resolve(path)?;
        if !fs.is_dir(id) {
            return Err(FsError::NotADirectory);
        }
        let names = fs.list(id)?;
        let mut rows: Vec<(String, bool)> = Vec::new();
        for n in names {
            let child_id = fs.resolve_from(id, &n)?;
            rows.push((n, fs.is_dir(child_id)));
        }
        Ok((rows, fs.path_of(id)))
    })
}
