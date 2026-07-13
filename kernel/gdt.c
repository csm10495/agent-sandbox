#include "kernel.h"

/*
 * Global Descriptor Table and Task State Segment for ring-3 support.
 *
 * The selector layout is chosen so the SYSCALL/SYSRET MSRs line up:
 *   0x08 kernel code, 0x10 kernel data, 0x18 user data, 0x20 user code.
 * SYSCALL loads CS/SS from the kernel pair; ring-3 entry and interrupt return
 * use the user pair. The TSS supplies RSP0, the stack the CPU switches to when
 * an interrupt or exception is taken while running in ring 3.
 */

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

#define GDT_ENTRIES 7 /* null, kcode, kdata, udata, ucode, tss(2 slots) */
static uint64_t gdt[GDT_ENTRIES];
static tss_t tss;

/* A dedicated stack the CPU switches to for ring-3 faults and the one the
 * SYSCALL entry stub loads. The kernel is not reentrant on this path. */
#define KERNEL_STACK_SIZE 16384
static uint8_t kernel_stack[KERNEL_STACK_SIZE] __attribute__((aligned(16)));

uintptr_t syscall_kernel_stack_top;

static void set_tss_descriptor(int index, uintptr_t base, uint32_t limit) {
    uint64_t low = 0;
    low |= (uint64_t)(limit & 0xffff);
    low |= (uint64_t)(base & 0xffffff) << 16;
    low |= (uint64_t)0x89 << 40;                 /* present, type = 64-bit TSS */
    low |= (uint64_t)((limit >> 16) & 0xf) << 48;
    low |= (uint64_t)((base >> 24) & 0xff) << 56;
    gdt[index] = low;
    gdt[index + 1] = (base >> 32) & 0xffffffff;
}

void tss_set_kernel_stack(uintptr_t stack_top) { tss.rsp0 = stack_top; }

void gdt_init(void) {
    gdt[0] = 0;
    gdt[1] = 0x00af9a000000ffffULL; /* 0x08 kernel code64, DPL0 */
    gdt[2] = 0x00af92000000ffffULL; /* 0x10 kernel data,   DPL0 */
    gdt[3] = 0x00aff2000000ffffULL; /* 0x18 user data,     DPL3 */
    gdt[4] = 0x00affa000000ffffULL; /* 0x20 user code64,   DPL3 */

    memset(&tss, 0, sizeof(tss));
    syscall_kernel_stack_top = (uintptr_t)&kernel_stack[KERNEL_STACK_SIZE];
    tss.rsp0 = syscall_kernel_stack_top;
    tss.iomap_base = sizeof(tss);
    set_tss_descriptor(5, (uintptr_t)&tss, sizeof(tss) - 1);

    gdt_ptr_t ptr = {.limit = sizeof(gdt) - 1, .base = (uintptr_t)gdt};
    __asm__ volatile("lgdt %0" : : "m"(ptr));
    __asm__ volatile("ltr %w0" : : "r"((uint16_t)0x28));
}
