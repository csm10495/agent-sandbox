#ifndef SABLEOS_PROC_H
#define SABLEOS_PROC_H

#include "kernel.h"

/*
 * Internal interface shared between the process loader (process.c) and the
 * Linux system call layer (syscall.c).
 */

#define MAX_FDS 16

/* A file descriptor is either the shared console (stdin/stdout/stderr) or a
 * read-only view of a ramfs file. */
typedef struct {
    bool in_use;
    bool is_console;
    const char *data;
    size_t size;
    size_t offset;
} file_desc_t;

typedef struct {
    address_space_t *space;
    uintptr_t brk_start;
    uintptr_t brk_cur;
    uintptr_t brk_max;
    uintptr_t mmap_next; /* anonymous mmap arena, grows down */
    uintptr_t mmap_floor;
    int exit_status;
    bool exited;
    file_desc_t fds[MAX_FDS];
} process_t;

/* Register frame the SYSCALL entry stub builds on the kernel stack. The field
 * order matches the push sequence in arch/x86_64/user_entry.S. */
typedef struct {
    uint64_t r15, r14, r13, r12, rbp, rbx, r9, r8, r10, rdx, rsi, rdi, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} syscall_regs_t;

extern process_t *current_process;

long syscall_dispatch(syscall_regs_t *regs);

/* Implemented in assembly (arch/x86_64/user_entry.S). enter_user does not
 * return through the normal path; control resumes at its call site via
 * kernel_return, which delivers the process status in the return register. */
long enter_user(uintptr_t entry_rip, uintptr_t user_rsp);
void kernel_return(uint64_t status) __attribute__((noreturn));

#endif
