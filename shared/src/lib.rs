//! Pure-logic modules shared between SandboxOS kernel and host-side unit tests.
//!
//! Everything in this crate is `no_std` but uses `alloc`. This crate is built
//! for both `x86_64-unknown-none` (kernel) and the host target (tests).
#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

pub mod bitmap;
pub mod ramfs;
pub mod shell;
