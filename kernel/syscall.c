#include "proc.h"

/*
 * Linux x86_64 system call layer.
 *
 * Enough of the ABI is implemented to start glibc-static and BusyBox-static
 * programs and run single-process workloads: console and ramfs-backed file
 * I/O, heap growth via brk, anonymous mmap, TLS setup through arch_prctl, and
 * the pile of identity/limit/signal calls a C runtime performs at startup.
 * Calls that would require multi-process support (fork/clone/execve/wait4)
 * return -ENOSYS so callers fail gracefully instead of misbehaving.
 */

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_stat 4
#define SYS_fstat 5
#define SYS_lstat 6
#define SYS_lseek 8
#define SYS_mmap 9
#define SYS_mprotect 10
#define SYS_munmap 11
#define SYS_brk 12
#define SYS_rt_sigaction 13
#define SYS_rt_sigprocmask 14
#define SYS_ioctl 16
#define SYS_writev 20
#define SYS_access 21
#define SYS_sched_yield 24
#define SYS_dup 32
#define SYS_dup2 33
#define SYS_getpid 39
#define SYS_sendfile 40
#define SYS_clone 56
#define SYS_fork 57
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_kill 62
#define SYS_uname 63
#define SYS_fcntl 72
#define SYS_getcwd 79
#define SYS_readlink 89
#define SYS_sigaltstack 131
#define SYS_getuid 102
#define SYS_getgid 104
#define SYS_setuid 105
#define SYS_setgid 106
#define SYS_geteuid 107
#define SYS_getegid 108
#define SYS_getppid 110
#define SYS_prctl 157
#define SYS_arch_prctl 158
#define SYS_gettid 186
#define SYS_tkill 200
#define SYS_set_tid_address 218
#define SYS_clock_gettime 228
#define SYS_exit_group 231
#define SYS_tgkill 234
#define SYS_openat 257
#define SYS_newfstatat 262
#define SYS_readlinkat 267
#define SYS_set_robust_list 273
#define SYS_prlimit64 302
#define SYS_getrandom 318
#define SYS_rseq 334

#define EPERM 1
#define ENOENT 2
#define EBADF 9
#define ENOMEM 12
#define EINVAL 22
#define ENOSYS 38
#define ENOTTY 25

#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003

#define AT_FDCWD (-100)
#define O_CLOEXEC 0x80000

#define S_IFREG 0100000
#define S_IFCHR 0020000

#define MAP_ANONYMOUS 0x20

struct linux_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    uint64_t st_atime;
    uint64_t st_atime_nsec;
    uint64_t st_mtime;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t __unused[3];
};

struct linux_iovec {
    void *iov_base;
    size_t iov_len;
};

struct linux_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

static uint64_t rng_state;

extern void syscall_entry(void);

static void wrmsr(uint32_t msr, uint64_t value) {
    __asm__ volatile("wrmsr"
                     :
                     : "c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32)));
}

static uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

void syscall_init(void) {
    /* Enable SYSCALL/SYSRET (EFER.SCE) while preserving long mode (LME). */
    wrmsr(0xc0000080, rdmsr(0xc0000080) | 1);
    /* STAR: SYSCALL loads kernel CS 0x08/SS 0x10; the user pair is 0x20/0x18. */
    wrmsr(0xc0000081, ((uint64_t)0x0010 << 48) | ((uint64_t)0x0008 << 32));
    wrmsr(0xc0000082, (uint64_t)(uintptr_t)syscall_entry); /* LSTAR entry */
    wrmsr(0xc0000084, 0x700); /* FMASK: clear TF, IF, DF on entry */
}

static uint64_t rdtsc(void) {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

static uint64_t next_random(void) {
    if (!rng_state) rng_state = rdtsc() | 1;
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static void copy_cstr(char *dst, const char *src, size_t cap) {
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = src[i];
    dst[i] = 0;
}

static const char *basename_of(const char *path) {
    const char *name = path;
    for (const char *p = path; *p; p++)
        if (*p == '/') name = p + 1;
    return name;
}

static int alloc_fd(void) {
    for (int i = 0; i < MAX_FDS; i++)
        if (!current_process->fds[i].in_use) return i;
    return -1;
}

static long do_write(int fd, const char *buf, size_t count) {
    if (fd < 0 || fd >= MAX_FDS || !current_process->fds[fd].in_use) return -EBADF;
    file_desc_t *f = &current_process->fds[fd];
    if (!f->is_console) return -EBADF; /* files are read-only here */
    for (size_t i = 0; i < count; i++) console_putc(buf[i]);
    return (long)count;
}

static long do_read(int fd, char *buf, size_t count) {
    if (fd < 0 || fd >= MAX_FDS || !current_process->fds[fd].in_use) return -EBADF;
    file_desc_t *f = &current_process->fds[fd];
    if (f->is_console) {
        if (count == 0) return 0;
        char c = keyboard_read();
        buf[0] = c;
        return 1;
    }
    size_t remaining = f->size - f->offset;
    if (count > remaining) count = remaining;
    memcpy(buf, f->data + f->offset, count);
    f->offset += count;
    return (long)count;
}

static long do_openat(const char *path, int flags) {
    (void)flags;
    const char *name = basename_of(path);
    const ramfs_file_t *file = ramfs_find(name);
    if (!file) return -ENOENT;
    int fd = alloc_fd();
    if (fd < 0) return -ENOMEM;
    file_desc_t *f = &current_process->fds[fd];
    f->in_use = true;
    f->is_console = false;
    f->data = file->data;
    f->size = file->size;
    f->offset = 0;
    return fd;
}

static void fill_stat(struct linux_stat *st, int fd) {
    memset(st, 0, sizeof(*st));
    file_desc_t *f = &current_process->fds[fd];
    st->st_blksize = 4096;
    if (f->is_console) {
        st->st_mode = S_IFCHR | 0620;
        st->st_rdev = 0x0500; /* pretend to be a tty device */
    } else {
        st->st_mode = S_IFREG | 0644;
        st->st_size = (int64_t)f->size;
        st->st_blocks = (int64_t)((f->size + 511) / 512);
    }
    st->st_nlink = 1;
}

static long do_fstat(int fd, struct linux_stat *st) {
    if (fd < 0 || fd >= MAX_FDS || !current_process->fds[fd].in_use) return -EBADF;
    fill_stat(st, fd);
    return 0;
}

static long do_brk(uintptr_t addr) {
    process_t *p = current_process;
    if (addr == 0) return (long)p->brk_cur;
    if (addr < p->brk_start || addr > p->brk_max) return (long)p->brk_cur;
    if (addr > p->brk_cur) {
        if (vmm_map_user_alloc(p->space, p->brk_cur,
                               addr - p->brk_cur, true, false) != 0)
            return (long)p->brk_cur;
    }
    p->brk_cur = addr;
    return (long)p->brk_cur;
}

static long do_mmap(uintptr_t addr, size_t length, uint64_t flags, int fd) {
    (void)addr;
    process_t *p = current_process;
    if (!(flags & MAP_ANONYMOUS) || fd != -1) return -ENOSYS;
    size_t len = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (len == 0) return -EINVAL;
    uintptr_t base = p->mmap_next - len;
    if (base < p->mmap_floor || base < p->brk_cur) return -ENOMEM;
    if (vmm_map_user_alloc(p->space, base, len, true, true) != 0) return -ENOMEM;
    p->mmap_next = base;
    return (long)base;
}

static long do_ioctl(int fd, uint64_t request, void *arg) {
    if (fd < 0 || fd >= MAX_FDS || !current_process->fds[fd].in_use) return -EBADF;
    if (current_process->fds[fd].is_console && request == 0x5401 /* TCGETS */) {
        /* glibc's tcgetattr()/isatty() passes a struct __kernel_termios, which
         * is 36 bytes on x86-64 (4 tcflag_t + c_line + cc_t c_cc[19]); writing
         * more than that overflows the caller's on-stack buffer. */
        if (arg) memset(arg, 0, 36);
        return 0;
    }
    return -ENOTTY;
}

static long do_uname(struct linux_utsname *u) {
    memset(u, 0, sizeof(*u));
    copy_cstr(u->sysname, "Linux", sizeof(u->sysname));
    copy_cstr(u->nodename, "sableos", sizeof(u->nodename));
    copy_cstr(u->release, "6.0.0-sable", sizeof(u->release));
    copy_cstr(u->version, "SableOS Linux ABI", sizeof(u->version));
    copy_cstr(u->machine, "x86_64", sizeof(u->machine));
    return 0;
}

static long do_prlimit64(int resource, void *old_limit) {
    if (!old_limit) return 0;
    uint64_t *limits = old_limit; /* rlim_cur, rlim_max */
    if (resource == 3 /* RLIMIT_STACK */) {
        limits[0] = USER_STACK_SIZE;
        limits[1] = ~0ULL;
    } else {
        limits[0] = ~0ULL;
        limits[1] = ~0ULL;
    }
    return 0;
}

static long do_getrandom(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)next_random();
    return (long)len;
}

static long do_getcwd(char *buf, size_t size) {
    if (!buf || size < 2) return -EINVAL;
    buf[0] = '/';
    buf[1] = 0;
    return 2; /* Linux returns the length of the path including the NUL. */
}

static long do_arch_prctl(int code, uintptr_t addr) {
    if (code == ARCH_SET_FS) {
        __asm__ volatile("wrmsr" : : "c"(0xc0000100), "a"((uint32_t)addr),
                         "d"((uint32_t)(addr >> 32)));
        return 0;
    }
    return -EINVAL;
}

static long do_writev(int fd, const struct linux_iovec *iov, int count) {
    long total = 0;
    for (int i = 0; i < count; i++) {
        long n = do_write(fd, iov[i].iov_base, iov[i].iov_len);
        if (n < 0) return total ? total : n;
        total += n;
    }
    return total;
}

static long do_sendfile(int out_fd, int in_fd, int64_t *offset, size_t count) {
    if (in_fd < 0 || in_fd >= MAX_FDS || !current_process->fds[in_fd].in_use)
        return -EBADF;
    file_desc_t *f = &current_process->fds[in_fd];
    if (f->is_console) return -EINVAL;
    size_t start = offset ? (size_t)*offset : f->offset;
    if (start > f->size) return -EINVAL;
    size_t remaining = f->size - start;
    if (count > remaining) count = remaining;
    long written = do_write(out_fd, f->data + start, count);
    if (written < 0) return written;
    if (offset) *offset = (int64_t)(start + written);
    else f->offset = start + written;
    return written;
}

static void report_unimplemented(long nr) {
    console_write("[sableos: unimplemented syscall ");
    console_write_dec((uint64_t)nr);
    console_write("]\n");
}

long syscall_dispatch(syscall_regs_t *regs) {
    long nr = (long)regs->rax;
    long a1 = (long)regs->rdi, a2 = (long)regs->rsi, a3 = (long)regs->rdx;
    long a4 = (long)regs->r10, a5 = (long)regs->r8;
    long result;

#ifdef SYSCALL_TRACE
    console_write("<sc ");
    console_write_dec((uint64_t)nr);
    console_write(">");
#endif

    switch (nr) {
    case SYS_read:
        result = do_read((int)a1, (char *)a2, (size_t)a3);
        break;
    case SYS_write:
        result = do_write((int)a1, (const char *)a2, (size_t)a3);
        break;
    case SYS_writev:
        result = do_writev((int)a1, (const struct linux_iovec *)a2, (int)a3);
        break;
    case SYS_open:
        result = do_openat((const char *)a1, (int)a2);
        break;
    case SYS_openat:
        result = do_openat((const char *)a2, (int)a3);
        break;
    case SYS_close:
        if (a1 >= 0 && a1 < MAX_FDS && current_process->fds[a1].in_use &&
            !current_process->fds[a1].is_console) {
            current_process->fds[a1].in_use = false;
            result = 0;
        } else if (a1 >= 0 && a1 < MAX_FDS && current_process->fds[a1].is_console) {
            result = 0; /* keep the console open */
        } else {
            result = -EBADF;
        }
        break;
    case SYS_lseek: {
        if (a1 < 0 || a1 >= MAX_FDS || !current_process->fds[a1].in_use ||
            current_process->fds[a1].is_console) {
            result = -EBADF;
            break;
        }
        file_desc_t *f = &current_process->fds[a1];
        size_t pos = a3 == 1 ? f->offset + a2 : a3 == 2 ? f->size + a2 : (size_t)a2;
        if (pos > f->size) pos = f->size;
        f->offset = pos;
        result = (long)pos;
        break;
    }
    case SYS_fstat:
    case SYS_newfstatat:
        result = do_fstat(nr == SYS_fstat ? (int)a1 : (int)a2,
                          (struct linux_stat *)(nr == SYS_fstat ? a2 : a3));
        break;
    case SYS_stat: {
        const ramfs_file_t *file = ramfs_find(basename_of((const char *)a1));
        if (!file) {
            result = -ENOENT;
            break;
        }
        struct linux_stat *st = (struct linux_stat *)a2;
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFREG | 0644;
        st->st_size = (int64_t)file->size;
        st->st_blksize = 4096;
        st->st_nlink = 1;
        result = 0;
        break;
    }
    case SYS_access:
        result = ramfs_find(basename_of((const char *)a1)) ? 0 : -ENOENT;
        break;
    case SYS_brk:
        result = do_brk((uintptr_t)a1);
        break;
    case SYS_mmap:
        result = do_mmap((uintptr_t)a1, (size_t)a2, (uint64_t)a5 /* flags */,
                         (int)regs->r9 /* fd */);
        break;
    case SYS_munmap:
    case SYS_mprotect:
        result = 0;
        break;
    case SYS_ioctl:
        result = do_ioctl((int)a1, (uint64_t)a2, (void *)a3);
        break;
    case SYS_arch_prctl:
        result = do_arch_prctl((int)a1, (uintptr_t)a2);
        break;
    case SYS_uname:
        result = do_uname((struct linux_utsname *)a1);
        break;
    case SYS_prlimit64:
        result = do_prlimit64((int)a2, (void *)a4);
        break;
    case SYS_getrandom:
        result = do_getrandom((uint8_t *)a1, (size_t)a2);
        break;
    case SYS_getcwd:
        result = do_getcwd((char *)a1, (size_t)a2);
        break;
    case SYS_kill:
    case SYS_tkill:
    case SYS_tgkill: {
        /* A process signalling itself with a fatal signal (abort() uses
         * tgkill+SIGABRT) terminates with the conventional 128+signo status
         * instead of faulting on an unhandled instruction afterwards. */
        int sig = (nr == SYS_tgkill) ? (int)a3 : (int)a2;
        if (sig > 0) {
            current_process->exit_status = 128 + sig;
            current_process->exited = true;
            kernel_return((uint64_t)(128 + sig));
        }
        result = 0;
        break;
    }
    case SYS_sendfile:
        result = do_sendfile((int)a1, (int)a2, (int64_t *)a3, (size_t)a4);
        break;
    case SYS_set_tid_address:
    case SYS_gettid:
    case SYS_getpid:
        result = 1;
        break;
    case SYS_getppid:
        result = 0;
        break;
    case SYS_set_robust_list:
    case SYS_prctl:
    case SYS_rt_sigaction:
    case SYS_rt_sigprocmask:
    case SYS_sigaltstack:
    case SYS_sched_yield:
    case SYS_getuid:
    case SYS_getgid:
    case SYS_geteuid:
    case SYS_getegid:
    case SYS_setuid:
    case SYS_setgid:
        result = 0;
        break;
    case SYS_clock_gettime: {
        uint64_t *ts = (uint64_t *)a2;
        if (ts) {
            ts[0] = 0;
            ts[1] = 0;
        }
        result = 0;
        break;
    }
    case SYS_dup:
    case SYS_dup2:
    case SYS_fcntl:
        result = (nr == SYS_dup2) ? a2 : a1;
        break;
    case SYS_readlink:
    case SYS_readlinkat:
        result = -ENOENT;
        break;
    case SYS_rseq:
    case SYS_clone:
    case SYS_fork:
    case SYS_execve:
    case SYS_wait4:
        result = -ENOSYS;
        break;
    case SYS_exit:
    case SYS_exit_group:
        current_process->exit_status = (int)a1;
        current_process->exited = true;
        kernel_return((uint64_t)(int)a1);
    default:
        report_unimplemented(nr);
        result = -ENOSYS;
        break;
    }

    regs->rax = (uint64_t)result;
    return result;
}
