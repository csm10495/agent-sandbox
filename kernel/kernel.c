/* kernel/kernel.c - Kernel main entry point */
#include "types.h"
#include "vga.h"
#include "string.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "memory.h"
#include "fs.h"
#include "thread.h"
#include "smp.h"
#include "shell.h"

/* Linker-provided symbols */
extern char kernel_phys_end[];

/* Serial port (COM1) for debug output and boot tests */
#define COM1 0x3F8
static void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* disable interrupts */
    outb(COM1 + 3, 0x80);   /* enable DLAB */
    outb(COM1 + 0, 0x03);   /* 38400 baud divisor lo */
    outb(COM1 + 1, 0x00);   /* divisor hi */
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);   /* FIFO */
    outb(COM1 + 4, 0x0B);   /* enable */
}
static void serial_putchar(char c) {
    while (!(inb(COM1 + 5) & 0x20));
    outb(COM1, c);
}
static void serial_puts(const char *s) {
    while (*s) { if (*s == '\n') serial_putchar('\r'); serial_putchar(*s++); }
}

/* Banner printed on boot */
static void print_banner(void) {
    vga_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts(
        " __  __ _           ___  ____\n"
        "|  \\/  (_)  ___ _ _/ _ \\/ ___|\n"
        "| |\\/| | | / __| '_| | | \\___ \\\n"
        "| |  | | || (__| | | |_| |___) |\n"
        "|_|  |_|_| \\___|_|  \\___/|____/  v0.1\n"
    );
    vga_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  amd64 | in-repo bootloader | freestanding C kernel\n\n");
}

/* Background demo thread */
static void idle_thread(void *arg) {
    (void)arg;
    for (;;) {
        thread_sleep_ms(200);
        thread_yield();
    }
}

void kernel_main(uint32_t magic, uint64_t mbi_addr) {
    /* BSS is already zeroed by entry.asm before calling us */

    /* Init VGA and serial FIRST */
    vga_init();
    serial_init();

    print_banner();
    serial_puts("[KERNEL] MicrOS kernel started\n");

    vga_printf("[BOOT] magic=0x%08x  mbi=0x%lx\n", magic, mbi_addr);

    /* IDT and interrupt controllers */
    idt_init();
    pic_init();
    /* Mask all IRQs initially; subsystems unmask what they need */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    /* Timer */
    pit_init(100);   /* 100 Hz */
    serial_puts("[KERNEL] PIT 100 Hz\n");

    /* Keyboard */
    keyboard_init();
    serial_puts("[KERNEL] Keyboard ready\n");

    /* Physical memory */
    uintptr_t kend = (uintptr_t)kernel_phys_end;
    if (kend < 0x200000) kend = 0x200000;
    pmm_init(kend);
    vga_printf("[MEM] %lu KB free (%lu pages)\n",
               pmm_free_pages() * 4, pmm_free_pages());
    serial_puts("[KERNEL] PMM ready\n");

    /* Filesystem */
    fs_init();
    serial_puts("[KERNEL] FS ready\n");

    /* Enable interrupts */
    cpu_sti();

    /* Threads */
    thread_init();
    thread_create(idle_thread, NULL, "idle");
    serial_puts("[KERNEL] Threads ready\n");

    /* SMP */
    smp_init();
    serial_puts("[KERNEL] SMP init done\n");

    vga_printf("[KERNEL] Kernel ready. %d CPU(s) detected.\n\n",
               smp_cpu_count());
    serial_puts("[KERNEL] Kernel ready\n");

    /* Enter the interactive shell (main thread becomes shell) */
    shell_run(fs_root());

    /* Should not reach here */
    serial_puts("[KERNEL] Shell exited\n");
    cpu_cli();
    for (;;) cpu_halt();
}
