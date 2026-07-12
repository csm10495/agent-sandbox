#ifndef SABLEOS_KERNEL_H
#define SABLEOS_KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int value, size_t n);
int memcmp(const void *a, const void *b, size_t n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *s);
void console_write_dec(uint64_t value);
void console_write_hex(uint64_t value);
void serial_init(void);
void serial_putc(char c);

void keyboard_init(void);
char keyboard_read(void);

#define RAMFS_MAX_FILES 32
#define RAMFS_NAME_MAX 31
#define RAMFS_DATA_MAX 512
typedef struct {
    bool used;
    char name[RAMFS_NAME_MAX + 1];
    char data[RAMFS_DATA_MAX + 1];
    size_t size;
} ramfs_file_t;

void ramfs_init(void);
int ramfs_create(const char *name);
int ramfs_write(const char *name, const char *data);
const ramfs_file_t *ramfs_find(const char *name);
int ramfs_remove(const char *name);
const ramfs_file_t *ramfs_at(size_t index);

void threads_init(void);
int thread_create(const char *name, void (*entry)(void));
void thread_yield(void);
size_t thread_count(void);
const char *thread_name(size_t index);

uint32_t cpu_discover(void);
void cpu_enable_lapic(void);

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#endif
