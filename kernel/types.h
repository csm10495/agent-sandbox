#pragma once
/* kernel/types.h - Fundamental type definitions for freestanding kernel.
 * When compiled with -DUNIT_TEST (host side), use standard headers instead.
 */

#ifdef UNIT_TEST
/* Host side: use system headers */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>   /* memset/memcpy etc. provided by host libc */
#else
/* Freestanding kernel: define our own types */

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;
typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef uint64_t            uintptr_t;
typedef int64_t             ptrdiff_t;

#define NULL  ((void *)0)
#define true  1
#define false 0
typedef _Bool bool;

#endif /* UNIT_TEST */

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define MIN(a, b)      ((a) < (b) ? (a) : (b))
#define MAX(a, b)      ((a) > (b) ? (a) : (b))

#ifndef UNIT_TEST
/* Port I/O (freestanding kernel only) */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void io_wait(void) { outb(0x80, 0); }

/* MMIO helpers */
static inline uint32_t mmio_read32(uintptr_t addr) {
    return *((volatile uint32_t *)addr);
}
static inline void mmio_write32(uintptr_t addr, uint32_t val) {
    *((volatile uint32_t *)addr) = val;
}

/* CPU helpers */
static inline void cpu_halt(void)  { __asm__ volatile ("hlt"); }
static inline void cpu_sti(void)   { __asm__ volatile ("sti"); }
static inline void cpu_cli(void)   { __asm__ volatile ("cli"); }
static inline void cpu_pause(void) { __asm__ volatile ("pause"); }

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"((uint32_t)val),
                                   "d"((uint32_t)(val >> 32)));
}
static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}
#else
/* Stubs for host-side unit tests */
static inline void outb(uint16_t p, uint8_t v)  { (void)p; (void)v; }
static inline uint8_t inb(uint16_t p)            { (void)p; return 0; }
static inline void io_wait(void)                 {}
static inline void cpu_halt(void)               {}
static inline void cpu_sti(void)                {}
static inline void cpu_cli(void)                {}
static inline void cpu_pause(void)              {}
#endif /* !UNIT_TEST */
