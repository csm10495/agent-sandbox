; kernel/thread_switch.asm
; Context-switch between kernel threads.

section .text

; void thread_switch_asm(uint64_t *save_rsp, uint64_t new_rsp)
;   rdi = address to store current RSP (from->saved_rsp)
;   rsi = RSP to load (to->saved_rsp)
global thread_switch_asm
thread_switch_asm:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    pushfq

    mov [rdi], rsp       ; save current stack pointer
    mov rsp,   rsi       ; switch to new stack

    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

; Thread entry trampoline.
; When a new thread is first scheduled, thread_switch_asm returns here.
; At that point: r12 = thread function, r13 = argument
extern thread_exit
global thread_trampoline
thread_trampoline:
    sti
    mov rdi, r13         ; pass argument
    call r12             ; call thread function
    call thread_exit     ; thread returned — clean up
.dead:
    cli
    hlt
    jmp .dead

section .note.GNU-stack noalloc noexec nowrite progbits
