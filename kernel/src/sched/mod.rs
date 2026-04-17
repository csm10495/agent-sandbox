//! Preemptive round-robin scheduler with native kernel threads.
//!
//! Design: single global run queue protected by a raw atomic spinlock
//! [`SCHED_LOCK`]. The lock is held across context switches — the incoming
//! thread releases the lock on the other side of [`switch_context`]. This
//! avoids a race where an "old" thread would otherwise be published on the
//! run queue with a stale RSP before `switch_context` finishes saving it.

use alloc::boxed::Box;
use alloc::collections::VecDeque;
use alloc::string::{String, ToString};
use alloc::sync::Arc;
use alloc::vec::Vec;
use core::sync::atomic::{AtomicBool, AtomicU64, Ordering};

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
    pub wake_tick: AtomicU64,
    pub cpu_ticks: AtomicU64,
    pub is_idle: bool,
}

unsafe impl Send for Thread {}
unsafe impl Sync for Thread {}

impl Drop for Thread {
    fn drop(&mut self) {
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

// State protected conceptually by SCHED_LOCK. The inner `Mutex`es are cheap
// and uncontended since SCHED_LOCK already serializes all access, but keeping
// them gives us safe `&mut` without more unsafe.
static RUN_QUEUE: Mutex<VecDeque<Arc<Thread>>> = Mutex::new(VecDeque::new());
static SLEEP_LIST: Mutex<Vec<Arc<Thread>>> = Mutex::new(Vec::new());
static ALL_THREADS: Mutex<Vec<Arc<Thread>>> = Mutex::new(Vec::new());

/// Raw spinlock: `true` = held.
static SCHED_LOCK: AtomicBool = AtomicBool::new(false);

fn sched_lock() {
    while SCHED_LOCK
        .compare_exchange_weak(false, true, Ordering::Acquire, Ordering::Relaxed)
        .is_err()
    {
        while SCHED_LOCK.load(Ordering::Relaxed) {
            core::hint::spin_loop();
        }
    }
}

fn sched_unlock() {
    SCHED_LOCK.store(false, Ordering::Release);
}

/// Called by [`thread_trampoline`] (asm) before a freshly-created thread runs
/// user code. Releases the lock the creator-side `schedule` did not drop.
#[no_mangle]
pub extern "C" fn sched_post_switch_unlock() {
    sched_unlock();
}

pub struct CpuLocal {
    pub lapic_id: u32,
    pub current: Mutex<Option<Arc<Thread>>>,
    pub idle: Mutex<Option<Arc<Thread>>>,
    pub online: AtomicBool,
    pub ran_threads: AtomicU64,
    pub timer_irqs: AtomicU64,
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
        timer_irqs: AtomicU64::new(0),
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
    0
}

fn this_cpu() -> Arc<CpuLocal> {
    let idx = this_cpu_idx();
    CPUS.lock()[idx].clone()
}

pub fn total_ticks() -> u64 {
    TICKS.load(Ordering::Relaxed)
}

pub fn spawn<F>(name: &str, entry: F) -> u64
where
    F: FnOnce() + Send + 'static,
{
    let boxed: Box<dyn FnOnce() + Send> = Box::new(entry);
    let boxed_ptr: *mut Box<dyn FnOnce() + Send> = Box::into_raw(Box::new(boxed));

    let stack_size = DEFAULT_STACK_SIZE;
    let layout = core::alloc::Layout::from_size_align(stack_size, 16).unwrap();
    let stack_base = unsafe { alloc::alloc::alloc_zeroed(layout) };
    assert!(!stack_base.is_null(), "spawn: out of memory for thread stack");

    let sp = build_initial_stack(stack_base, stack_size, boxed_ptr);

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

    let _irq = IrqGuard::new();
    sched_lock();
    ALL_THREADS.lock().push(thread.clone());
    RUN_QUEUE.lock().push_back(thread);
    sched_unlock();
    tid
}

fn build_initial_stack(
    stack_base: *mut u8,
    stack_size: usize,
    arg: *mut Box<dyn FnOnce() + Send>,
) -> *mut u64 {
    let stack_top = unsafe { stack_base.add(stack_size) };
    let mut sp = ((stack_top as usize) & !0xF) as *mut u64;
    unsafe {
        sp = sp.sub(8);
        //  low address -- high address
        //   r15=0  r14=0  r13=arg  r12=shim  rbp=0  rbx=0  rflags=0x202  ret=trampoline
        *sp.add(7) = crate::arch::x86_64::context::thread_trampoline as u64;
        *sp.add(6) = 0x202;
        *sp.add(5) = 0;
        *sp.add(4) = 0;
        *sp.add(3) = thread_entry_shim as u64;
        *sp.add(2) = arg as u64;
        *sp.add(1) = 0;
        *sp.add(0) = 0;
    }
    sp
}

extern "C" fn thread_entry_shim(arg: *mut Box<dyn FnOnce() + Send>) {
    // SAFETY: built by `spawn`/`spawn_idle` via `Box::into_raw`.
    let boxed: Box<Box<dyn FnOnce() + Send>> = unsafe { Box::from_raw(arg) };
    let f: Box<dyn FnOnce() + Send> = *boxed;
    f();
}

#[no_mangle]
pub extern "C" fn thread_exit() -> ! {
    context::disable_interrupts();
    sched_lock();
    let cur = this_cpu().current.lock().clone();
    if let Some(t) = cur {
        *t.state.lock() = ThreadState::Exited;
    }
    schedule_locked_and_unlock();
    // Unreachable: exited threads are never scheduled again.
    loop {
        core::hint::spin_loop();
    }
}

pub fn yield_now() {
    let _irq = IrqGuard::new();
    sched_lock();
    schedule_locked_and_unlock();
}

pub fn sleep_ticks(ticks: u64) {
    {
        let _irq = IrqGuard::new();
        sched_lock();
        let cpu = this_cpu();
        let cur = cpu.current.lock().clone();
        if let Some(t) = cur {
            t.wake_tick
                .store(TICKS.load(Ordering::Relaxed) + ticks, Ordering::Relaxed);
            *t.state.lock() = ThreadState::Sleeping;
            SLEEP_LIST.lock().push(t);
        }
        schedule_locked_and_unlock();
    }
}

/// Called from the LAPIC timer IRQ handler. CPU has already disabled IRQs.
pub fn timer_tick() {
    this_cpu().timer_irqs.fetch_add(1, Ordering::Relaxed);
    sched_lock();
    if this_cpu_idx() == 0 {
        let t = TICKS.fetch_add(1, Ordering::Relaxed) + 1;
        let mut list = SLEEP_LIST.lock();
        let mut i = 0;
        while i < list.len() {
            if list[i].wake_tick.load(Ordering::Relaxed) <= t {
                let th = list.swap_remove(i);
                *th.state.lock() = ThreadState::Runnable;
                RUN_QUEUE.lock().push_back(th);
            } else {
                i += 1;
            }
        }
    }
    schedule_locked_and_unlock();
}

/// Precondition: SCHED_LOCK is held; interrupts are disabled.
/// Postcondition: SCHED_LOCK released (by this or a future CPU).
fn schedule_locked_and_unlock() {
    let cpu = this_cpu();

    let mut next = RUN_QUEUE.lock().pop_front();
    let old = cpu.current.lock().clone();

    if let Some(ref old) = old {
        let st = *old.state.lock();
        match st {
            ThreadState::Running => {
                if next.is_none() && !old.is_idle {
                    // Keep running same non-idle thread.
                    sched_unlock();
                    return;
                }
                *old.state.lock() = ThreadState::Runnable;
                if !old.is_idle {
                    // Safe to enqueue now: SCHED_LOCK prevents any other CPU
                    // from popping and consuming our stale RSP. The RSP will
                    // be fixed up by switch_context below, which completes
                    // BEFORE we release the lock (the incoming thread does).
                    RUN_QUEUE.lock().push_back(old.clone());
                }
            }
            ThreadState::Sleeping | ThreadState::Exited => {}
            ThreadState::Runnable => {
                if !old.is_idle {
                    RUN_QUEUE.lock().push_back(old.clone());
                }
            }
        }
    }

    if next.is_none() {
        if let Some(idle) = cpu.idle.lock().clone() {
            next = Some(idle);
        } else {
            sched_unlock();
            return;
        }
    }
    let next = next.unwrap();

    if let Some(ref old) = old {
        if Arc::ptr_eq(old, &next) {
            *next.state.lock() = ThreadState::Running;
            sched_unlock();
            return;
        }
    }

    *next.state.lock() = ThreadState::Running;
    next.cpu_ticks.fetch_add(1, Ordering::Relaxed);
    cpu.ran_threads.fetch_add(1, Ordering::Relaxed);

    let new_rsp = next.rsp.load(Ordering::Relaxed);
    *cpu.current.lock() = Some(next);

    match old {
        Some(old) => {
            let old_rsp_ptr = old.rsp.as_ptr() as *mut u64;
            // Lock is handed off: incoming thread releases.
            unsafe {
                context::switch_context(old_rsp_ptr, new_rsp);
            }
            // We've resumed. Whichever thread switched TO us held the lock;
            // we release it now.
            sched_unlock();
        }
        None => unsafe {
            // First-ever schedule on this CPU; no context to save.
            // The incoming thread will release the lock via
            // sched_post_switch_unlock in the trampoline.
            context::load_initial_context(new_rsp);
        },
    }
}

fn spawn_idle() -> Arc<Thread> {
    let boxed: Box<dyn FnOnce() + Send> = Box::new(|| loop {
        crate::arch::x86_64::idle_once();
    });
    let boxed_ptr: *mut Box<dyn FnOnce() + Send> = Box::into_raw(Box::new(boxed));

    let stack_size = 16 * 1024;
    let layout = core::alloc::Layout::from_size_align(stack_size, 16).unwrap();
    let stack_base = unsafe { alloc::alloc::alloc_zeroed(layout) };
    assert!(!stack_base.is_null());

    let sp = build_initial_stack(stack_base, stack_size, boxed_ptr);

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

/// Enter the scheduler for the first time on the current CPU.
pub fn enter(cpu_idx: usize) -> ! {
    let idle = spawn_idle();
    {
        let cpus = CPUS.lock();
        *cpus[cpu_idx].idle.lock() = Some(idle);
        cpus[cpu_idx].online.store(true, Ordering::Relaxed);
    }
    context::disable_interrupts();
    sched_lock();
    schedule_locked_and_unlock();
    loop {
        crate::arch::x86_64::idle_once();
    }
}

pub fn list_threads() -> Vec<(u64, String, ThreadState, u64)> {
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

pub fn cpu_ran_counts() -> Vec<(u32, u64, bool, u64)> {
    CPUS.lock()
        .iter()
        .map(|c| {
            (
                c.lapic_id,
                c.ran_threads.load(Ordering::Relaxed),
                c.online.load(Ordering::Relaxed),
                c.timer_irqs.load(Ordering::Relaxed),
            )
        })
        .collect()
}
