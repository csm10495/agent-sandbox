#pragma once
#include "types.h"

/* 64-bit IDT gate descriptor (16 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  ist;         /* bits [2:0] = IST, rest 0 */
    uint8_t  type_attr;   /* P|DPL(2)|0|type(4): 0x8E = interrupt gate */
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t reserved;
} idt_entry_t;

/* CPU register frame as pushed by isr_common_stub */
typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t isr_num, error_code;
    /* CPU-pushed fields (iretq frame): */
    uint64_t rip, cs, rflags, rsp, ss;
} interrupt_frame_t;

void idt_init(void);
void idt_set_handler(uint8_t vec, void (*handler)(void), uint8_t type_attr);
void irq_register(uint8_t irq, void (*handler)(void));

/* C-level dispatch called by isr_common_stub */
void interrupt_handler(interrupt_frame_t *frame);
