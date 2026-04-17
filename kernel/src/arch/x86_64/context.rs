//! x86_64 kernel thread context-switch primitives (Rust side).
//!
//! The actual assembly is in `context.rs` via `global_asm!`.

use core::arch::global_asm;

extern "C" {
    /// Save current callee-saved regs to the current stack, store the resulting
    /// stack pointer at `*old_rsp`, switch to `new_rsp`, restore callee-saved
    /// regs from the new stack, and return.
    ///
    /// SAFETY: Both stacks must be valid kernel stacks; the caller must have
    /// locked any necessary per-CPU state.
    pub fn switch_context(old_rsp: *mut u64, new_rsp: u64);

    /// Load a stack pointer and jump into it — used to start the very first
    /// thread on a CPU from an empty initial context.
    pub fn load_initial_context(new_rsp: u64) -> !;

    /// Trampoline that runs the first time a freshly-constructed thread is
    /// switched to.  Reads entry+arg from r12/r13, calls the entry, and then
    /// calls `thread_exit` if it ever returns.
    pub fn thread_trampoline();
}

global_asm!(
    r#"
    .section .text
    .global switch_context
    .type switch_context,@function
switch_context:
    pushfq
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [rdi], rsp
    mov rsp, rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    popfq
    ret

    .global load_initial_context
    .type load_initial_context,@function
load_initial_context:
    mov rsp, rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    popfq
    ret

    .global thread_trampoline
    .type thread_trampoline,@function
thread_trampoline:
    # r12 = entry fn, r13 = arg
    mov rdi, r13
    call r12
    # If the entry returns, fall through to thread_exit.
    call thread_exit
    # Should never return; halt if it does.
1:
    hlt
    jmp 1b
    "#
);

#[inline(always)]
pub fn enable_interrupts() {
    unsafe { core::arch::asm!("sti", options(nomem, nostack, preserves_flags)) };
}

#[inline(always)]
pub fn disable_interrupts() {
    unsafe { core::arch::asm!("cli", options(nomem, nostack, preserves_flags)) };
}

#[inline(always)]
pub fn halt() {
    unsafe { core::arch::asm!("hlt", options(nomem, nostack, preserves_flags)) };
}

#[inline(always)]
pub fn pause() {
    core::hint::spin_loop();
}

#[inline(always)]
pub fn are_interrupts_enabled() -> bool {
    let rflags: u64;
    unsafe {
        core::arch::asm!(
            "pushfq; pop {}",
            out(reg) rflags,
            options(nomem, preserves_flags)
        );
    }
    rflags & (1 << 9) != 0
}

/// Interrupt-flag guard: disables IRQs for the duration, restores prior state.
pub struct IrqGuard {
    was_enabled: bool,
}

impl IrqGuard {
    pub fn new() -> Self {
        let was_enabled = are_interrupts_enabled();
        disable_interrupts();
        Self { was_enabled }
    }
}

impl Drop for IrqGuard {
    fn drop(&mut self) {
        if self.was_enabled {
            enable_interrupts();
        }
    }
}
