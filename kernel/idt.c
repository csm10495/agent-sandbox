#include "idt.h"
#include "vga.h"
#include "pic.h"
#include "pit.h"

/* Forward declarations for ISR stubs (generated in idt_stubs.asm) */
#define DECLARE_ISR(n) extern void isr_stub_##n(void);
DECLARE_ISR(0)  DECLARE_ISR(1)  DECLARE_ISR(2)  DECLARE_ISR(3)
DECLARE_ISR(4)  DECLARE_ISR(5)  DECLARE_ISR(6)  DECLARE_ISR(7)
DECLARE_ISR(8)  DECLARE_ISR(9)  DECLARE_ISR(10) DECLARE_ISR(11)
DECLARE_ISR(12) DECLARE_ISR(13) DECLARE_ISR(14) DECLARE_ISR(15)
DECLARE_ISR(16) DECLARE_ISR(17) DECLARE_ISR(18) DECLARE_ISR(19)
DECLARE_ISR(20) DECLARE_ISR(21) DECLARE_ISR(22) DECLARE_ISR(23)
DECLARE_ISR(24) DECLARE_ISR(25) DECLARE_ISR(26) DECLARE_ISR(27)
DECLARE_ISR(28) DECLARE_ISR(29) DECLARE_ISR(30) DECLARE_ISR(31)
DECLARE_ISR(32) DECLARE_ISR(33) DECLARE_ISR(34) DECLARE_ISR(35)
DECLARE_ISR(36) DECLARE_ISR(37) DECLARE_ISR(38) DECLARE_ISR(39)
DECLARE_ISR(40) DECLARE_ISR(41) DECLARE_ISR(42) DECLARE_ISR(43)
DECLARE_ISR(44) DECLARE_ISR(45) DECLARE_ISR(46) DECLARE_ISR(47)
DECLARE_ISR(128) /* syscall vector */

static idt_entry_t idt[256] __attribute__((aligned(16)));

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

static idtr_t idtr;

void idt_set_handler(uint8_t vec, void (*handler)(void), uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;
    idt[vec].offset_lo  = (uint16_t)(addr & 0xFFFF);
    idt[vec].selector   = 0x08;      /* kernel code segment */
    idt[vec].ist        = 0;
    idt[vec].type_attr  = type_attr;
    idt[vec].offset_mid = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vec].offset_hi  = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vec].reserved   = 0;
}

void idt_init(void) {
    /* Register all 48 hardware vectors */
#define SET(n) idt_set_handler(n, isr_stub_##n, 0x8E)
    SET(0);  SET(1);  SET(2);  SET(3);  SET(4);  SET(5);
    SET(6);  SET(7);  SET(8);  SET(9);  SET(10); SET(11);
    SET(12); SET(13); SET(14); SET(15); SET(16); SET(17);
    SET(18); SET(19); SET(20); SET(21); SET(22); SET(23);
    SET(24); SET(25); SET(26); SET(27); SET(28); SET(29);
    SET(30); SET(31);
    SET(32); SET(33); SET(34); SET(35); SET(36); SET(37);
    SET(38); SET(39); SET(40); SET(41); SET(42); SET(43);
    SET(44); SET(45); SET(46); SET(47);
#undef SET

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)idt;
    __asm__ volatile ("lidt %0" :: "m"(idtr));
}

/* Exception names */
static const char *exc_names[] = {
    "Divide Error","Debug","NMI","Breakpoint","Overflow",
    "Bound Range","Invalid Opcode","Device N/A","Double Fault",
    "Coprocessor","Invalid TSS","Segment NP","Stack Fault",
    "General Protection","Page Fault","Reserved","x87 FP",
    "Alignment Check","Machine Check","SIMD FP","Virtualisation",
    "Control Prot","Res22","Res23","Res24","Res25","Res26",
    "Res27","Res28","HV Inj","VMM Comm","Security"
};

/* IRQ dispatch table */
typedef void (*irq_handler_t)(void);
static irq_handler_t irq_handlers[16];

void irq_register(uint8_t irq, irq_handler_t h) {
    if (irq < 16) irq_handlers[irq] = h;
}

/* Central interrupt dispatch */
void interrupt_handler(interrupt_frame_t *frame) {
    uint64_t num = frame->isr_num;

    if (num < 32) {
        /* CPU exception */
        vga_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        vga_printf("\n[EXCEPTION #%lu: %s] err=0x%lx RIP=0x%lx\n",
                   num, (num < 32 ? exc_names[num] : "?"),
                   frame->error_code, frame->rip);
        vga_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
        /* Halt on unrecoverable exceptions */
        cpu_cli();
        for (;;) cpu_halt();
    } else if (num < 48) {
        /* IRQ */
        uint8_t irq = (uint8_t)(num - 32);
        if (irq == 0) {
            /* Timer */
            pit_tick();
        } else if (irq_handlers[irq]) {
            irq_handlers[irq]();
        }
        pic_eoi(irq);
    }
}
