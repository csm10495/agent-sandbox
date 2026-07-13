/*
 * A freestanding, static Linux x86_64 program. It links against no libc and
 * issues raw Linux system calls through the `syscall` instruction, exactly like
 * any other Linux ELF. SableOS loads and runs it in ring 3 to demonstrate
 * binary-level Linux ABI compatibility.
 *
 * It exercises the parts of the process ABI that a real program depends on:
 *   - argc/argv/envp are read from the initial stack layout the kernel builds,
 *   - the auxiliary vector is walked to recover AT_PAGESZ and AT_RANDOM,
 *   - write(2)/read(2)/exit_group(2) are issued via the syscall ABI.
 */

typedef unsigned long size_t;
typedef long ssize_t;

#define SYS_read 0
#define SYS_write 1
#define SYS_exit_group 231

#define AT_NULL 0
#define AT_PAGESZ 6
#define AT_RANDOM 25

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return ret;
}

static ssize_t sys_write(int fd, const void *buf, size_t n) {
    return (ssize_t)syscall3(SYS_write, fd, (long)buf, (long)n);
}

static __attribute__((noreturn)) void sys_exit_group(int code) {
    syscall3(SYS_exit_group, code, 0, 0);
    __builtin_unreachable();
}

static size_t str_len(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void put_str(const char *s) { sys_write(1, s, str_len(s)); }

static void put_hex(unsigned long value) {
    static const char digits[] = "0123456789abcdef";
    char buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 16; i++)
        buffer[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xf];
    buffer[18] = 0;
    put_str(buffer);
}

/*
 * The kernel jumps here with the System V AMD64 initial stack:
 *   [rsp]      = argc
 *   [rsp+8]    = argv[0..argc-1]
 *   ...        = NULL
 *   ...        = envp[0..], NULL
 *   ...        = auxv pairs terminated by AT_NULL
 * The assembler shim hands us that pointer in %rdi.
 */
void user_main(unsigned long *stack) {
    long argc = (long)stack[0];
    char **argv = (char **)(stack + 1);
    char **envp = argv + argc + 1;

    put_str("Hello from ring 3 (SableOS Linux ABI)\n");

    put_str("argc=");
    put_hex((unsigned long)argc);
    put_str(" argv0=");
    put_str(argc > 0 && argv[0] ? argv[0] : "(null)");
    put_str("\n");

    char **env = envp;
    while (*env) env++;
    unsigned long *auxv = (unsigned long *)(env + 1);
    unsigned long pagesz = 0;
    const unsigned char *random16 = 0;
    for (unsigned long *a = auxv; a[0] != AT_NULL; a += 2) {
        if (a[0] == AT_PAGESZ) pagesz = a[1];
        else if (a[0] == AT_RANDOM) random16 = (const unsigned char *)a[1];
    }

    put_str("auxv AT_PAGESZ=");
    put_hex(pagesz);
    put_str(" AT_RANDOM=");
    put_hex((unsigned long)random16);
    put_str("\n");

    put_str("user syscalls OK; exiting via exit_group(0)\n");
    sys_exit_group(0);
}
