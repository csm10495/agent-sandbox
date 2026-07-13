/*
 * Process entry point for freestanding SableOS user programs.
 *
 * The kernel enters at _start with the System V AMD64 initial process stack:
 * %rsp points at argc, the ABI-required 16-byte stack alignment is in effect,
 * and %rdx holds a function pointer for atexit registration (unused here).
 * We hand the raw stack pointer to user_main and never return.
 */
.section .text
.global _start
.type _start, @function
_start:
    xor %rbp, %rbp
    mov %rsp, %rdi            /* argument: pointer to argc/argv/envp/auxv */
    and $-16, %rsp            /* keep the stack 16-byte aligned for the ABI */
    call user_main
    /* user_main exits via exit_group; guard against a stray return. */
    mov $60, %rax            /* SYS_exit */
    xor %rdi, %rdi
    syscall
1:
    jmp 1b

.section .note.GNU-stack,"",@progbits
