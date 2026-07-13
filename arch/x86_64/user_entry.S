/*
 * Ring-3 transition, SYSCALL entry, and CPU exception stubs.
 *
 * The kernel shares one CR3 with the running process (kernel memory is
 * identity-mapped in every address space), so the fast SYSCALL path only has
 * to switch stacks, spill the user register file, and call into C. Return to
 * user mode uses IRETQ, which avoids the canonical-address pitfalls of SYSRET.
 */

.section .bss
.align 16
saved_user_rsp:
    .skip 8
/* Saved kernel execution context for the longjmp-style return out of ring 3:
 * return address, rsp, rbx, rbp, r12, r13, r14, r15. */
.global kernel_ctx
kernel_ctx:
    .skip 64

.section .text
.code64

/* void enter_user(uint64_t entry_rip, uint64_t user_rsp); does not return here.
 * Control comes back through kernel_return() when the process exits or faults. */
.global enter_user
.type enter_user, @function
enter_user:
    mov (%rsp), %rax
    mov %rax, kernel_ctx+0(%rip)      /* return address */
    lea 8(%rsp), %rax
    mov %rax, kernel_ctx+8(%rip)      /* caller rsp after ret */
    mov %rbx, kernel_ctx+16(%rip)
    mov %rbp, kernel_ctx+24(%rip)
    mov %r12, kernel_ctx+32(%rip)
    mov %r13, kernel_ctx+40(%rip)
    mov %r14, kernel_ctx+48(%rip)
    mov %r15, kernel_ctx+56(%rip)

    push $0x1b                        /* user SS (RPL 3) */
    push %rsi                         /* user RSP */
    push $0x2                         /* RFLAGS: reserved bit, IF cleared */
    push $0x23                        /* user CS (RPL 3) */
    push %rdi                         /* user RIP */

    xor %rax, %rax
    xor %rbx, %rbx
    xor %rcx, %rcx
    xor %rdx, %rdx
    xor %rsi, %rsi
    xor %rdi, %rdi
    xor %rbp, %rbp
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15
    iretq

/* void kernel_return(uint64_t status); resumes enter_user's caller. */
.global kernel_return
.type kernel_return, @function
kernel_return:
    mov %rdi, %rax                    /* propagate status as enter_user return */
    mov kernel_ctx+8(%rip), %rsp
    mov kernel_ctx+16(%rip), %rbx
    mov kernel_ctx+24(%rip), %rbp
    mov kernel_ctx+32(%rip), %r12
    mov kernel_ctx+40(%rip), %r13
    mov kernel_ctx+48(%rip), %r14
    mov kernel_ctx+56(%rip), %r15
    mov kernel_ctx+0(%rip), %rcx
    jmp *%rcx

/* SYSCALL entry loaded into LSTAR. rcx=user rip, r11=user rflags. */
.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    mov %rsp, saved_user_rsp(%rip)
    mov syscall_kernel_stack_top(%rip), %rsp

    push $0x1b                        /* SS   */
    push saved_user_rsp(%rip)         /* RSP  */
    push %r11                         /* RFLAGS */
    push $0x23                        /* CS   */
    push %rcx                         /* RIP  */

    push %rax
    push %rdi
    push %rsi
    push %rdx
    push %r10
    push %r8
    push %r9
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, %rdi
    call syscall_dispatch

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    pop %r9
    pop %r8
    pop %r10
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rax
    iretq

/* Exception stubs. Vectors that do not push an error code push a zero so the
 * frame layout is uniform. */
.macro EXC_NOERR vec
.global isr\vec
isr\vec:
    push $0
    push $\vec
    jmp exception_common
.endm

.macro EXC_ERR vec
.global isr\vec
isr\vec:
    push $\vec
    jmp exception_common
.endm

EXC_NOERR 0
EXC_NOERR 1
EXC_NOERR 2
EXC_NOERR 3
EXC_NOERR 4
EXC_NOERR 5
EXC_NOERR 6
EXC_NOERR 7
EXC_ERR   8
EXC_NOERR 9
EXC_ERR   10
EXC_ERR   11
EXC_ERR   12
EXC_ERR   13
EXC_ERR   14
EXC_NOERR 15
EXC_NOERR 16
EXC_ERR   17
EXC_NOERR 18
EXC_NOERR 19
EXC_NOERR 20
EXC_ERR   21
EXC_NOERR 22
EXC_NOERR 23
EXC_NOERR 24
EXC_NOERR 25
EXC_NOERR 26
EXC_NOERR 27
EXC_NOERR 28
EXC_ERR   29
EXC_ERR   30
EXC_NOERR 31

exception_common:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    mov %rsp, %rdi
    call exception_handler
1:
    hlt
    jmp 1b

.section .note.GNU-stack,"",@progbits
