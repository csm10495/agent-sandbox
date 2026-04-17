//! Preemptive round-robin scheduler with native kernel threads.
//!
//! A single global run queue is protected by a spinlock. Every CPU has its own
//! idle thread and its own "current thread" pointer. The LAPIC timer fires on
//! every CPU; on each tick we try to pull a runnable thread from the global
//! queue and switch to it. When a thread exits or yields, it goes back to the
//! queue (or is dropped, in the case of exit).
//!
//! This is deliberately the simplest correct multi-core scheduler: one global
//! queue + per-CPU idle threads. It's demonstrably SMP because multiple CPUs
//! pull from the same queue concurrently (with the lock serializing access).

use alloc::boxed::Box;
use alloc::collections::VecDeque;
use alloc::string::String;
use alloc::string::ToString;
use alloc::sync::Arc;
use alloc::vec::Vec;
use core::sync::atomic::{AtomicBool, AtomicU32, AtomicU64, AtomicUsize, Ordering};

use spin::Mutex;

use crate::arch::x86_64::context::{self, IrqGuard};
use crate::arch::x86_64::lapic;

pub const DEFAULT_STACK_SIZE: usize = 64 * 1024;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ThreadState {
    Runnable,
    Running,
    Sleeping,
    Exited,
}

pub struct Thread {
    pub id: u64,
    pub name: String,
    pub rsp: AtomicU64,
    pub state: Mutex<ThreadState>,
    pub stack_base: *mut u8,
    pub stack_size: usize,
    /// Monotonic tick at which a sleeping thread should be woken.
    pub wake_tick: AtomicU64,
    pub cpu_ticks: AtomicU64,
    pub is_idle: bool,
}

// Safety: Thread is shared across CPUs; all fields are atomic/locked or
// point to owned heap storage that is never freed while live.
unsafe impl Send for Thread {}
unsafe impl Sync for Thread {}

impl Drop for Thread {
    fn drop(&mut self) {
        // Free the stack we allocated on spawn.
        if !self.stack_base.is_null() {
            unsafe {
                let layout = core::alloc::Layout::from_size_align(self.stack_size, 16).unwrap();
                alloc::alloc::dealloc(self.stack_base, layout);
            }
        }
    }
}

static NEXT_TID: AtomicU64 = AtomicU64::new(1);
static TICKS: AtomicU64 = AtomicU64::new(0);

/// Global run queue of runnable threads.
static RUN_QUEUE: Mutex<VecDeque<Arc<Thread>>> = Mutex::new(VecDeque::new());

/// Sleeping threads, to be polled each tick.
static SLEEP_LIST: Mutex<Vec<Arc<Thread>>> = Mutex::new(Vec::new());

/// All live threads (for introspection via `ps`).
static ALL_THREADS: Mutex<Vec<Arc<Thread>>> = Mutex::new(Vec::new());

/// Per-CPU state. Indexed by CPU index (0..NUM_CPUS).
pub struct CpuLocal {
    pub lapic_id: u32,
    pub current: Mutex<Option<Arc<Thread>>>,
    pub idle: Mutex<Option<Arc<Thread>>>,
    pub online: AtomicBool,
    pub ran_threads: AtomicU64,
}

static CPUS: Mutex<Vec<Arc<CpuLocal>>> = Mutex::new(Vec::new());

pub fn register_cpu(lapic_id: u32) -> usize {
    let mut cpus = CPUS.lock();
    let idx = cpus.len();
    cpus.push(Arc::new(CpuLocal {
        lapic_id,
        current: Mutex::new(None),
        idle: Mutex::new(None),
        online: AtomicBool::new(false),
        ran_threads: AtomicU64::new(0),
    }));
    idx
}

pub fn num_cpus() -> usize {
    CPUS.lock().len()
}

pub fn num_online_cpus() -> usize {
    CPUS.lock()
        .iter()
        .filter(|c| c.online.load(Ordering::Relaxed))
        .count()
}

fn this_cpu_idx() -> usize {
    let id = lapic::id();
    let cpus = CPUS.lock();
    for (i, c) in cpus.iter().enumerate() {
        if c.lapic_id == id {
            return i;
        }
    }
    // If we haven't registered this CPU yet, return 0; in practice registration
    // happens before any scheduling runs.
    0
}

fn this_cpu() -> Arc<CpuLocal> {
    let idx = this_cpu_idx();
    CPUS.lock()[idx].clone()
}

pub fn total_ticks() -> u64 {
    TICKS.load(Ordering::Relaxed)
}

/// Create and enqueue a new kernel thread.
pub fn spawn<F>(name: &str, entry: F) -> u64
where
    F: FnOnce() + Send + 'static,
{
    // Box the closure into a heap-allocated, erased callable.
    let boxed: Box<dyn FnOnce() + Send> = Box::new(entry);
    let boxed_ptr: *mut Box<dyn FnOnce() + Send> =
        Box::into_raw(Box::new(boxed));

    let stack_size = DEFAULT_STACK_SIZE;
    let layout = core::alloc::Layout::from_size_align(stack_size, 16).unwrap();
    let stack_base = unsafe { alloc::alloc::alloc_zeroed(layout) };
    assert!(!stack_base.is_null(), "spawn: out of memory for thread stack");

    // Set up initial stack so that `switch_context` will return into
    // `thread_trampoline` with r12=entry-fn-ptr, r13=arg.
    let stack_top = unsafe { stack_base.add(stack_size) };
    // Align down to 16, then reserve 8 bytes so that after `ret` the stack is
    // 16-byte aligned at the first instruction of the trampoline (System V).
    let mut sp = ((stack_top as usize) & !0xF) as *mut u64;
    unsafe {
        // Stack order from top to bottom (pushed by ctx switch in reverse):
        //   [sp+56] return address  -> thread_trampoline
        //   [sp+48] rflags           -> 0x202 (IF=1, reserved bit)
        //   [sp+40] rbx              -> 0
        //   [sp+32] rbp              -> 0
        //   [sp+24] r12              -> thread_entry_shim  (receives closure ptr in rdi)
        //   [sp+16] r13              -> closure ptr
        //   [sp+8 ] r14              -> 0
        //   [sp+0 ] r15              -> 0
        sp = sp.sub(8);
        *sp.add(7) = crate::arch::x86_64::context::thread_trampoline as u64;
        *sp.add(6) = 0x202;
        *sp.add(5) = 0; // rbx
        *sp.add(4) = 0; // rbp
        *sp.add(3) = thread_entry_shim as u64; // r12
        *sp.add(2) = boxed_ptr as u64; // r13
        *sp.add(1) = 0; // r14
        *sp.add(0) = 0; // r15
    }

    let tid = NEXT_TID.fetch_add(1, Ordering::Relaxed);
    let thread = Arc::new(Thread {
        id: tid,
        name: name.to_string(),
        rsp: AtomicU64::new(sp as u64),
        state: Mutex::new(ThreadState::Runnable),
        stack_base,
        stack_size,
        wake_tick: AtomicU64::new(0),
        cpu_ticks: AtomicU64::new(0),
        is_idle: false,
    });

    ALL_THREADS.lock().push(thread.clone());
    RUN_QUEUE.lock().push_back(thread);
    tid
}

/// Trampoline-called shim: `rdi = *mut Box<dyn FnOnce()+Send>`.
///
/// SAFETY: called exactly once per thread, with a valid pointer produced by
/// `Box::into_raw`.
extern "C" fn thread_entry_shim(arg: *mut Box<dyn FnOnce() + Send>) {
    // Thread starts with interrupts enabled (popf set IF=1 for us), so the
    // LAPIC timer can preempt us.
    let boxed: Box<Box<dyn FnOnce() + Send>> = unsafe { Box::from_raw(arg) };
    let f: Box<dyn FnOnce() + Send> = *boxed;
    f();
}

/// Called by a thread when it voluntarily wants to exit.
#[no_mangle]
pub extern "C" fn thread_exit() -> ! {
    // Mark current thread as exited, then yield. The scheduler will not put it
    // back into the run queue.
    {
        let _guard = IrqGuard::new();
        let cur = this_cpu().current.lock().clone();
        if let Some(t) = cur {
            *t.state.lock() = ThreadState::Exited;
        }
    }
    // Yield forever. After we switch away, we will not come back because we're
    // exited. Just spin (unreachable).
    loop {
        yield_now();
    }
}

/// Voluntarily yield the CPU.
pub fn yield_now() {
    let _guard = IrqGuard::new();
    schedule();
}

/// Sleep for `ticks` scheduler ticks.
pub fn sleep_ticks(ticks: u64) {
    {
        let _guard = IrqGuard::new();
        let cpu = this_cpu();
        let cur = cpu.current.lock().clone();
        if let Some(t) = cur {
            t.wake_tick
                .store(TICKS.load(Ordering::Relaxed) + ticks, Ordering::Relaxed);
            *t.state.lock() = ThreadState::Sleeping;
            SLEEP_LIST.lock().push(t);
        }
    }
    yield_now();
}

/// Called from the LAPIC timer IRQ handler.
pub fn timer_tick() {
    // Only CPU 0 increments the global tick counter; that way "uptime" is
    // monotonic and SMP-consistent.
    if this_cpu_idx() == 0 {
        let t = TICKS.fetch_add(1, Ordering::Relaxed) + 1;
        // Wake any sleeping threads.
        let mut woken: Vec<Arc<Thread>> = Vec::new();
        {
            let mut list = SLEEP_LIST.lock();
            let mut i = 0;
            while i < list.len() {
                if list[i].wake_tick.load(Ordering::Relaxed) <= t {
                    let th = list.swap_remove(i);
                    *th.state.lock() = ThreadState::Runnable;
                    woken.push(th);
                } else {
                    i += 1;
                }
            }
        }
        if !woken.is_empty() {
            let mut q = RUN_QUEUE.lock();
            for th in woken {
                q.push_back(th);
            }
        }
    }
    // Re-enter the scheduler to pick next thread.
    schedule();
}

/// Core scheduling routine. Must be called with interrupts disabled.
fn schedule() {
    let cpu = this_cpu();

    // Pull next runnable thread (may be None).
    let mut next = RUN_QUEUE.lock().pop_front();

    // Inspect / update old thread state.
    let old = cpu.current.lock().clone();
    if let Some(ref old) = old {
        let mut st = old.state.lock();
        match *st {
            ThreadState::Running => {
                if next.is_none() && !old.is_idle {
                    // No candidate; keep running the same thread.
                    return;
                }
                if old.is_idle {
                    // Never enqueue the idle thread.
                    *st = ThreadState::Runnable;
                } else {
                    *st = ThreadState::Runnable;
                    drop(st);
                    RUN_QUEUE.lock().push_back(old.clone());
                }
            }
            ThreadState::Sleeping | ThreadState::Exited => {
                // Already moved elsewhere; leave as-is.
            }
            ThreadState::Runnable => {
                // Unusual: old was runnable but not on queue. Put it back.
                if !old.is_idle {
                    drop(st);
                    RUN_QUEUE.lock().push_back(old.clone());
                }
            }
        }
    }

    // If still nothing to run, fall back to idle.
    if next.is_none() {
        if let Some(idle) = cpu.idle.lock().clone() {
            next = Some(idle);
        } else {
            return; // nothing we can do
        }
    }
    let next = next.unwrap();

    *next.state.lock() = ThreadState::Running;
    next.cpu_ticks.fetch_add(1, Ordering::Relaxed);
    cpu.ran_threads.fetch_add(1, Ordering::Relaxed);

    let new_rsp = next.rsp.load(Ordering::Relaxed);
    *cpu.current.lock() = Some(next);

    match old {
        Some(old) => {
            let old_rsp_ptr = old.rsp.as_ptr() as *mut u64;
            unsafe {
                context::switch_context(old_rsp_ptr, new_rsp);
            }
        }
        None => unsafe {
            context::load_initial_context(new_rsp);
        },
    }
}

/// Create an idle thread bound to a specific CPU. Not placed on the run queue.
fn spawn_idle() -> Arc<Thread> {
    let boxed: Box<dyn FnOnce() + Send> = Box::new(|| loop {
        crate::arch::x86_64::idle_once();
    });
    let boxed_ptr: *mut Box<dyn FnOnce() + Send> = Box::into_raw(Box::new(boxed));

    let stack_size = 16 * 1024;
    let layout = core::alloc::Layout::from_size_align(stack_size, 16).unwrap();
    let stack_base = unsafe { alloc::alloc::alloc_zeroed(layout) };
    assert!(!stack_base.is_null());

    let stack_top = unsafe { stack_base.add(stack_size) };
    let mut sp = ((stack_top as usize) & !0xF) as *mut u64;
    unsafe {
        sp = sp.sub(8);
        *sp.add(7) = crate::arch::x86_64::context::thread_trampoline as u64;
        *sp.add(6) = 0x202;
        *sp.add(5) = 0;
        *sp.add(4) = 0;
        *sp.add(3) = thread_entry_shim as u64;
        *sp.add(2) = boxed_ptr as u64;
        *sp.add(1) = 0;
        *sp.add(0) = 0;
    }

    let tid = NEXT_TID.fetch_add(1, Ordering::Relaxed);
    Arc::new(Thread {
        id: tid,
        name: "idle".to_string(),
        rsp: AtomicU64::new(sp as u64),
        state: Mutex::new(ThreadState::Runnable),
        stack_base,
        stack_size,
        wake_tick: AtomicU64::new(0),
        cpu_ticks: AtomicU64::new(0),
        is_idle: true,
    })
}

/// Enter the scheduler for the first time on the current CPU. Creates an idle
/// thread for this CPU and then drives the scheduler forever.
pub fn enter(cpu_idx: usize) -> ! {
    let idle = spawn_idle();
    {
        let cpus = CPUS.lock();
        *cpus[cpu_idx].idle.lock() = Some(idle);
        cpus[cpu_idx].online.store(true, Ordering::Relaxed);
    }
    loop {
        {
            let _guard = IrqGuard::new();
            schedule();
        }
        // Unreachable once we have scheduled into any thread (even idle) —
        // this halt is a safety net before the first schedule.
        crate::arch::x86_64::idle_once();
    }
}

/// Snapshot thread info for `ps`.
pub fn list_threads() -> Vec<(u64, String, ThreadState, u64)> {
    // Prune exited ones lazily.
    let mut all = ALL_THREADS.lock();
    all.retain(|t| *t.state.lock() != ThreadState::Exited);
    all.iter()
        .map(|t| {
            (
                t.id,
                t.name.clone(),
                *t.state.lock(),
                t.cpu_ticks.load(Ordering::Relaxed),
            )
        })
        .collect()
}

pub fn cpu_ran_counts() -> Vec<(u32, u64, bool)> {
    CPUS.lock()
        .iter()
        .map(|c| {
            (
                c.lapic_id,
                c.ran_threads.load(Ordering::Relaxed),
                c.online.load(Ordering::Relaxed),
            )
        })
        .collect()
}

// Silence unused-import warning when features differ.
#[allow(dead_code)]
fn _touch(_: AtomicU32, _: AtomicUsize) {}
