#include "kernel.h"

/*
 * Interrupt Descriptor Table. The kernel runs with maskable interrupts off and
 * polls its devices, so the IDT exists only to catch CPU exceptions. A fault in
 * ring 3 is turned into a clean process termination; a fault in ring 0 is a
 * kernel bug and halts the machine with a diagnostic.
 */

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, err, rip, cs, rflags, rsp, ss;
} exc_frame_t;

static struct idt_entry idt[256];

extern void kernel_return(uint64_t status);

#define ISR(n) extern void isr##n(void)
ISR(0); ISR(1); ISR(2); ISR(3); ISR(4); ISR(5); ISR(6); ISR(7);
ISR(8); ISR(9); ISR(10); ISR(11); ISR(12); ISR(13); ISR(14); ISR(15);
ISR(16); ISR(17); ISR(18); ISR(19); ISR(20); ISR(21); ISR(22); ISR(23);
ISR(24); ISR(25); ISR(26); ISR(27); ISR(28); ISR(29); ISR(30); ISR(31);
#undef ISR

static void (*const isr_table[32])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static void set_gate(int vector, void (*handler)(void)) {
    uintptr_t addr = (uintptr_t)handler;
    idt[vector].offset_low = addr & 0xffff;
    idt[vector].selector = 0x08; /* kernel code */
    idt[vector].ist = 0;
    idt[vector].type_attr = 0x8e; /* present, DPL0, 64-bit interrupt gate */
    idt[vector].offset_mid = (addr >> 16) & 0xffff;
    idt[vector].offset_high = (addr >> 32) & 0xffffffff;
    idt[vector].reserved = 0;
}

void idt_init(void) {
    memset(idt, 0, sizeof(idt));
    for (int i = 0; i < 32; i++) set_gate(i, isr_table[i]);
    struct idt_ptr ptr = {.limit = sizeof(idt) - 1, .base = (uintptr_t)idt};
    __asm__ volatile("lidt %0" : : "m"(ptr));
    /* Mask the legacy PIC so no maskable IRQ can reach an unhandled vector. */
    outb(0x21, 0xff);
    outb(0xa1, 0xff);
}

static const char *exception_name(uint64_t vector) {
    switch (vector) {
    case 0: return "divide error";
    case 6: return "invalid opcode";
    case 8: return "double fault";
    case 13: return "general protection";
    case 14: return "page fault";
    default: return "exception";
    }
}

void exception_handler(exc_frame_t *frame) {
    bool from_user = (frame->cs & 3) == 3;
    console_write("\n[");
    console_write(from_user ? "user" : "kernel");
    console_write(" fault] vector=");
    console_write_dec(frame->vector);
    console_write(" (");
    console_write(exception_name(frame->vector));
    console_write(") err=");
    console_write_hex(frame->err);
    console_write(" rip=");
    console_write_hex(frame->rip);
    if (frame->vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        console_write(" cr2=");
        console_write_hex(cr2);
    }
    console_putc('\n');

    if (from_user) {
        /* 128 + a synthetic signal number, mirroring shell conventions. */
        kernel_return(128 + (frame->vector == 14 ? 11 : 6));
    }
    console_write("kernel exception; halting.\n");
    for (;;) __asm__ volatile("cli; hlt");
}
