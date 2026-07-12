; kernel/idt_stubs.asm
; Generates 256 ISR entry stubs that push (error_code, isr_num) and jump
; to isr_common_stub. Exceptions 8,10-14,17,21,29,30 push their own
; hardware error code; all others push a dummy zero first.

%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push qword 0       ; dummy error code
    push qword %1      ; interrupt number
    jmp  isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    ; CPU already pushed error code
    push qword %1      ; interrupt number
    jmp  isr_common_stub
%endmacro

ISR_NOERR  0    ; #DE  Division Error
ISR_NOERR  1    ; #DB  Debug
ISR_NOERR  2    ;      NMI
ISR_NOERR  3    ; #BP  Breakpoint
ISR_NOERR  4    ; #OF  Overflow
ISR_NOERR  5    ; #BR  Bound Range
ISR_NOERR  6    ; #UD  Invalid Opcode
ISR_NOERR  7    ; #NM  Device Not Available
ISR_ERR    8    ; #DF  Double Fault
ISR_NOERR  9    ;      Coprocessor Segment Overrun (legacy)
ISR_ERR   10    ; #TS  Invalid TSS
ISR_ERR   11    ; #NP  Segment Not Present
ISR_ERR   12    ; #SS  Stack-Segment Fault
ISR_ERR   13    ; #GP  General Protection Fault
ISR_ERR   14    ; #PF  Page Fault
ISR_NOERR 15    ;      Reserved
ISR_NOERR 16    ; #MF  x87 Floating-Point
ISR_ERR   17    ; #AC  Alignment Check
ISR_NOERR 18    ; #MC  Machine Check
ISR_NOERR 19    ; #XF  SIMD FP Exception
ISR_NOERR 20    ; #VE  Virtualisation
ISR_ERR   21    ; #CP  Control Protection
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29    ; #HV  (AMD)
ISR_ERR   30    ; #SX  (AMD)
ISR_NOERR 31

; IRQs remapped to 0x20-0x2F
%assign i 32
%rep 16
    ISR_NOERR i
    %assign i i+1
%endrep

; Software interrupts / spare 48-255
%assign i 48
%rep 208
    ISR_NOERR i
    %assign i i+1
%endrep

; ----------------------------------------------------------------
; Common ISR stub - saves full CPU state, calls C handler, restores
; ----------------------------------------------------------------
section .text
extern interrupt_handler

isr_common_stub:
    ; Stack at entry: [rsp] = isr_num, [rsp+8] = error_code,
    ;   then CPU-pushed: rip, cs, rflags, rsp, ss

    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to register frame as first argument
    mov  rdi, rsp
    call interrupt_handler

    ; Restore registers
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rdx
    pop  rcx
    pop  rbx
    pop  rax

    ; Remove isr_num and error_code
    add  rsp, 16
    iretq
section .note.GNU-stack noalloc noexec nowrite progbits
